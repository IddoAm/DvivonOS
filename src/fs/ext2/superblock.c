/*
 * ext2/superblock.c – Reading, validating, and managing the ext2
 *                     on-disk superblock and block-group descriptors.
 *                     Contains the ext2_mount() entry-point that the
 *                     VFS calls when mounting an ext2 volume.
 */

#include <fs/ext2/ext2.h>
#include <fs/ext2/superblock.h>
#include <fs/ext2/block.h>
#include <fs/ext2/inode.h>
#include <fs/vfs/superblock.h>
#include <fs/vfs/inode.h>
#include <drivers/block_device.h>
#include <kernel/heap-allocator.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ */
/*  Metadata persistence helpers                                      */
/* ------------------------------------------------------------------ */

/*
 * Write the in-memory superblock back to its on-disk location.
 * For 1024-byte block-size the superblock occupies block 1 entirely.
 * For larger block sizes it sits at byte-offset 1024 inside block 0
 * and we must do a read-modify-write.
 */
int ext2_flush_superblock(superblock_t *sb)
{
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;

    if (fs->block_size == 1024) {
        return ext2_write_block(fs->dev, 1, fs->sb);
    }

    /* block_size > 1024: superblock is at offset 1024 inside block 0 */
    uint8_t *buf = (uint8_t *)kmalloc(fs->block_size);
    if (!buf) return -1;
    ext2_read_block(fs->dev, 0, buf);
    memcpy(buf + EXT2_SUPERBLOCK_OFFSET, fs->sb, sizeof(ext2_superblock_t));
    int ret = ext2_write_block(fs->dev, 0, buf);
    kfree((uintptr_t)buf);
    return ret;
}

/*
 * Write a single group descriptor back to the group-descriptor table
 * on disk.  The table starts at block (s_first_data_block + 1).
 */
int ext2_flush_group_desc(superblock_t *sb, uint32_t group)
{
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;
    uint32_t gd_per_block = fs->block_size / sizeof(ext2_group_desc_t);
    uint32_t gd_block = fs->sb->s_first_data_block + 1 + group / gd_per_block;

    uint8_t *buf = (uint8_t *)kmalloc(fs->block_size);
    if (!buf) return -1;

    ext2_read_block(fs->dev, gd_block, buf);
    uint32_t off = (group % gd_per_block) * sizeof(ext2_group_desc_t);
    memcpy(buf + off, &fs->group_desc[group], sizeof(ext2_group_desc_t));
    int ret = ext2_write_block(fs->dev, gd_block, buf);
    kfree((uintptr_t)buf);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  Superblock read / validate                                        */
/* ------------------------------------------------------------------ */

/*
 * Read the 1024-byte ext2 superblock from its fixed position at
 * byte-offset 1024 on the device.  At this point the device's
 * block_size is still 512 (sector mode), so we read 2 sectors.
 */
int ext2_read_superblock(block_device_t *dev, ext2_superblock_t *sb)
{
    if (!dev || !sb) return -1;
    /* Sector 2-3 (bytes 1024-2047) at 512-byte block_size */
    return dev->ops->read_blocks(dev, 2, 2, sb);
}

/*
 * Validate magic, block size, and revision.
 * Returns 0 if acceptable, -1 otherwise.
 */
int ext2_validate_superblock(const ext2_superblock_t *sb)
{
    if (!sb) return -1;

    if (sb->s_magic != EXT2_MAGIC) {
        printf("[ext2] bad magic 0x%x (expected 0x%x)\n",
               sb->s_magic, EXT2_MAGIC);
        return -1;
    }

    uint32_t block_size = 1024U << sb->s_log_block_size;
    if (block_size < EXT2_MIN_BLOCK_SIZE ||
        block_size > EXT2_MAX_BLOCK_SIZE) {
        printf("[ext2] unsupported block size %d\n", block_size);
        return -1;
    }

    if (sb->s_rev_level > EXT2_DYNAMIC_REV) {
        printf("[ext2] unsupported revision %d\n", sb->s_rev_level);
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Group descriptors                                                 */
/* ------------------------------------------------------------------ */

/*
 * Read all group descriptors into the pre-allocated array.
 * Must be called AFTER dev->block_size has been updated to the
 * ext2 filesystem block size.
 */
int ext2_read_group_desc(block_device_t *dev, const ext2_superblock_t *sb,
                         ext2_group_desc_t *groups, uint32_t count)
{
    if (!dev || !sb || !groups || count == 0)
        return -1;

    uint32_t block_size = 1024U << sb->s_log_block_size;
    uint32_t gd_per_block = block_size / sizeof(ext2_group_desc_t);
    uint32_t gd_block = sb->s_first_data_block + 1;

    uint8_t *buf = (uint8_t *)kmalloc(block_size);
    if (!buf) return -1;

    uint32_t read = 0;
    while (read < count) {
        ext2_read_block(dev, gd_block, buf);
        uint32_t n = count - read;
        if (n > gd_per_block) n = gd_per_block;
        memcpy(&groups[read], buf, n * sizeof(ext2_group_desc_t));
        read += n;
        gd_block++;
    }

    kfree((uintptr_t)buf);
    return 0;
}

/*
 * Compute which block group an inode or block belongs to.
 * If inode != 0 the calculation is based on inode number;
 * otherwise it falls back to block number.
 */
uint32_t ext2_get_block_group(const ext2_superblock_t *sb,
                              ino_t inode, block_t block)
{
    if (inode != 0)
        return (inode - 1) / sb->s_inodes_per_group;
    return (block - sb->s_first_data_block) / sb->s_blocks_per_group;
}

/* ------------------------------------------------------------------ */
/*  Mount / fill-super                                                */
/* ------------------------------------------------------------------ */

/*
 * Fill a VFS superblock with data read from an ext2 device.
 * Allocates ext2_fs_data_t, reads the on-disk superblock and
 * group descriptors, sets the device block_size, and loads the
 * root inode.
 */
int ext2_fill_super(superblock_t *sb, block_device_t *dev)
{
    if (!sb || !dev) return -1;

    /* --- allocate ext2 private data -------------------------------- */
    ext2_fs_data_t *fs = (ext2_fs_data_t *)kmalloc(sizeof(ext2_fs_data_t));
    if (!fs) return -1;
    memset(fs, 0, sizeof(*fs));

    ext2_superblock_t *raw_sb =
        (ext2_superblock_t *)kmalloc(sizeof(ext2_superblock_t));
    if (!raw_sb) { kfree((uintptr_t)fs); return -1; }
    
    /* --- read and validate superblock ------------------------------ */
    if (ext2_read_superblock(dev, raw_sb) != 0) {
        printf("[ext2] failed to read superblock\n");
        kfree((uintptr_t)raw_sb); kfree((uintptr_t)fs);
        return -1;
    }
    if (ext2_validate_superblock(raw_sb) != 0) {
        kfree((uintptr_t)raw_sb); kfree((uintptr_t)fs);
        return -1;
    }

    /* --- derive computed fields ------------------------------------ */
    uint32_t block_size = 1024U << raw_sb->s_log_block_size;
    uint32_t inode_size = (raw_sb->s_rev_level >= EXT2_DYNAMIC_REV)
                              ? raw_sb->s_inode_size : 128;
    uint32_t groups_count =
        (raw_sb->s_blocks_count - raw_sb->s_first_data_block +
         raw_sb->s_blocks_per_group - 1) / raw_sb->s_blocks_per_group;

    fs->sb             = raw_sb;
    fs->block_size     = block_size;
    fs->groups_count   = groups_count;
    fs->inodes_per_block = block_size / inode_size;
    fs->itable_blocks  =
        (raw_sb->s_inodes_per_group * inode_size + block_size - 1) / block_size;
    fs->dev            = dev;

    /* Update the device's block_size so all further block I/O uses
       the filesystem block size instead of the raw sector size.       */
    dev->block_size = block_size;

    /* --- read group descriptors ------------------------------------ */
    fs->group_desc = (ext2_group_desc_t *)kmalloc(
        groups_count * sizeof(ext2_group_desc_t));
    if (!fs->group_desc) {
        kfree((uintptr_t)raw_sb); kfree((uintptr_t)fs);
        return -1;
    }
    if (ext2_read_group_desc(dev, raw_sb, fs->group_desc, groups_count) != 0) {
        printf("[ext2] failed to read group descriptors\n");
        kfree((uintptr_t)fs->group_desc);
        kfree((uintptr_t)raw_sb); kfree((uintptr_t)fs);
        return -1;
    }

    /* --- populate VFS superblock ----------------------------------- */
    sb->blocksize      = block_size;
    /* compute log2(block_size) */
    sb->blocksize_bits = 10 + raw_sb->s_log_block_size;
    sb->fs_data        = fs;
    sb->ops            = &ext2_sb_ops;

    /* --- load root inode (inode 2) --------------------------------- */
    sb->root = ext2_read_inode(sb, EXT2_ROOT_INO);
    if (!sb->root) {
        printf("[ext2] failed to read root inode\n");
        /* Clear pointers so superblock_free() won't touch freed memory */
        sb->fs_data = NULL;
        sb->ops     = NULL;
        kfree((uintptr_t)fs->group_desc);
        kfree((uintptr_t)raw_sb);
        kfree((uintptr_t)fs);
        return -1;
    }

    printf("[ext2] mounted: block_size=%d  groups=%d  inodes=%d  "
           "free_blocks=%d  free_inodes=%d\n",
           block_size, groups_count, raw_sb->s_inodes_count,
           raw_sb->s_free_blocks_count, raw_sb->s_free_inodes_count);
    return 0;
}

/*
 * Top-level mount entry point called by the VFS.
 * Allocates a VFS superblock, fills it, and returns it.
 */
superblock_t *ext2_mount(block_device_t *dev)
{
    if (!dev) return NULL;

    superblock_t *sb = superblock_new(dev->major, "ext2");
    if (!sb) return NULL;
    
    if (ext2_fill_super(sb, dev) != 0) {
        superblock_free(sb);
        return NULL;
    }
    return sb;
}

/* ------------------------------------------------------------------ */
/*  Cleanup / sync / statfs                                           */
/* ------------------------------------------------------------------ */

/* Release ext2-specific resources associated with a superblock. */
void ext2_put_superblock(superblock_t *sb)
{
    if (!sb || !sb->fs_data) return;
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;

    if (fs->group_desc) kfree((uintptr_t)fs->group_desc);
    if (fs->sb)         kfree((uintptr_t)fs->sb);

    fs->group_desc = NULL;
    fs->sb = NULL;
}

/* Flush dirty metadata to disk. */
int ext2_sync(superblock_t *sb)
{
    if (!sb || !sb->fs_data) return -1;
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;

    ext2_flush_superblock(sb);
    for (uint32_t g = 0; g < fs->groups_count; g++)
        ext2_flush_group_desc(sb, g);

    return 0;
}

/* Fill a simple statistics buffer (total/free blocks and inodes). */
int ext2_statfs(superblock_t *sb, void *stat)
{
    if (!sb || !sb->fs_data || !stat) return -1;
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;

    /* Pack into a uint32_t[6] array for simplicity:
       [0] total blocks  [1] free blocks
       [2] total inodes  [3] free inodes
       [4] block size    [5] blocks per group                   */
    uint32_t *s = (uint32_t *)stat;
    s[0] = fs->sb->s_blocks_count;
    s[1] = fs->sb->s_free_blocks_count;
    s[2] = fs->sb->s_inodes_count;
    s[3] = fs->sb->s_free_inodes_count;
    s[4] = fs->block_size;
    s[5] = fs->sb->s_blocks_per_group;
    return 0;
}
