/*
 * ext2/inode.c – Inode reading, writing, allocation, and deallocation
 *                for the ext2 filesystem.
 *
 * Raw inode I/O reads / writes the 128-byte on-disk ext2_inode_t
 * from the inode table of the appropriate block group.
 * The higher-level ext2_read_inode / ext2_write_inode bridge between
 * the VFS inode_t and the on-disk structure.
 */

#include <fs/ext2/ext2.h>
#include <fs/ext2/inode.h>
#include <fs/ext2/block.h>
#include <fs/ext2/superblock.h>
#include <fs/vfs/inode.h>
#include <fs/vfs/superblock.h>
#include <drivers/block_device.h>
#include <kernel/heap-allocator.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ */
/*  Location helpers                                                  */
/* ------------------------------------------------------------------ */

/*
 * Compute the disk block number that contains the raw inode entry
 * for the given inode number.
 */
uint32_t ext2_get_inode_block(const ext2_fs_data_t *fs, ino_t ino)
{
    if (!fs || ino == 0) return 0;

    uint32_t group = (ino - 1) / fs->sb->s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb->s_inodes_per_group;
    uint32_t inode_size = (fs->sb->s_rev_level >= EXT2_DYNAMIC_REV)
                              ? fs->sb->s_inode_size : 128;
    uint32_t block_offset = (index * inode_size) / fs->block_size;

    return fs->group_desc[group].bg_inode_table + block_offset;
}

/* ------------------------------------------------------------------ */
/*  Raw inode I/O (on-disk 128-byte structures)                       */
/* ------------------------------------------------------------------ */

/*
 * Read the raw ext2_inode_t from the inode table on disk.
 * Reads the containing block, then copies the inode bytes out.
 */
int ext2_read_inode_raw(block_device_t *dev, const ext2_fs_data_t *fs,
                        ino_t ino, ext2_inode_t *inode)
{
    if (!dev || !fs || !inode || ino == 0) return -1;

    uint32_t group = (ino - 1) / fs->sb->s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb->s_inodes_per_group;
    uint32_t inode_size = (fs->sb->s_rev_level >= EXT2_DYNAMIC_REV)
                              ? fs->sb->s_inode_size : 128;
    uint32_t inodes_per_block = fs->block_size / inode_size;
    uint32_t block_offset     = index / inodes_per_block;
    uint32_t offset_in_block  = (index % inodes_per_block) * inode_size;

    uint32_t blk = fs->group_desc[group].bg_inode_table + block_offset;

    uint8_t *buf = (uint8_t *)kmalloc(fs->block_size);
    if (!buf) return -1;

    int ret = ext2_read_block(dev, blk, buf);
    if (ret == 0)
        memcpy(inode, buf + offset_in_block, sizeof(ext2_inode_t));

    kfree((uintptr_t)buf);
    return ret;
}

/*
 * Write the raw ext2_inode_t back to disk (read-modify-write the
 * block that contains the inode entry).
 */
int ext2_write_inode_raw(block_device_t *dev, const ext2_fs_data_t *fs,
                         ino_t ino, const ext2_inode_t *inode)
{
    if (!dev || !fs || !inode || ino == 0) return -1;

    uint32_t group = (ino - 1) / fs->sb->s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb->s_inodes_per_group;
    uint32_t inode_size = (fs->sb->s_rev_level >= EXT2_DYNAMIC_REV)
                              ? fs->sb->s_inode_size : 128;
    uint32_t inodes_per_block = fs->block_size / inode_size;
    uint32_t block_offset     = index / inodes_per_block;
    uint32_t offset_in_block  = (index % inodes_per_block) * inode_size;

    uint32_t blk = fs->group_desc[group].bg_inode_table + block_offset;

    uint8_t *buf = (uint8_t *)kmalloc(fs->block_size);
    if (!buf) return -1;

    int ret = ext2_read_block(dev, blk, buf);
    if (ret != 0) { kfree((uintptr_t)buf); return ret; }

    memcpy(buf + offset_in_block, inode, sizeof(ext2_inode_t));
    ret = ext2_write_block(dev, blk, buf);
    kfree((uintptr_t)buf);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  VFS inode interface                                               */
/* ------------------------------------------------------------------ */

/*
 * Load an inode from disk, wrap it in a VFS inode_t, and attach
 * ext2_inode_data_t as the fs-private payload.
 */
inode_t *ext2_read_inode(superblock_t *sb, ino_t ino)
{
    if (!sb || ino == 0) return NULL;
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;

    /* Allocate ext2 private data */
    ext2_inode_data_t *ei =
        (ext2_inode_data_t *)kmalloc(sizeof(ext2_inode_data_t));
    if (!ei) return NULL;
    memset(ei, 0, sizeof(*ei));

    if (ext2_read_inode_raw(fs->dev, fs, ino, &ei->disk_inode) != 0) {
        kfree((uintptr_t)ei);
        return NULL;
    }

    ei->block_group    = (ino - 1) / fs->sb->s_inodes_per_group;
    ei->index_in_group = (ino - 1) % fs->sb->s_inodes_per_group;

    /* Create VFS inode */
    inode_t *inode = inode_new(sb, ino);
    if (!inode) {
        kfree((uintptr_t)ei);
        return NULL;
    }

    inode->mode    = ei->disk_inode.i_mode;
    inode->size    = ei->disk_inode.i_size;
    inode->uid     = ei->disk_inode.i_uid;
    inode->gid     = ei->disk_inode.i_gid;
    inode->nlink   = ei->disk_inode.i_links_count;
    inode->atime   = ei->disk_inode.i_atime;
    inode->mtime   = ei->disk_inode.i_mtime;
    inode->ctime   = ei->disk_inode.i_ctime;
    inode->blksize = fs->block_size;
    inode->blocks  = ei->disk_inode.i_blocks;
    inode->flags   = ei->disk_inode.i_flags;
    inode->fs_data = ei;
    inode->f_ops   = &ext2_file_ops;

    return inode;
}

/*
 * Sync a VFS inode's metadata back to disk.
 * Copies the VFS fields into the on-disk inode and writes it.
 */
int ext2_write_inode(inode_t *inode)
{
    if (!inode || !inode->sb || !inode->fs_data) return -1;

    ext2_fs_data_t  *fs = (ext2_fs_data_t *)inode->sb->fs_data;
    ext2_inode_data_t *ei = (ext2_inode_data_t *)inode->fs_data;

    /* VFS -> on-disk */
    ei->disk_inode.i_mode        = (uint16_t)inode->mode;
    ei->disk_inode.i_uid         = (uint16_t)inode->uid;
    ei->disk_inode.i_size        = inode->size;
    ei->disk_inode.i_gid         = (uint16_t)inode->gid;
    ei->disk_inode.i_links_count = (uint16_t)inode->nlink;
    ei->disk_inode.i_atime       = inode->atime;
    ei->disk_inode.i_mtime       = inode->mtime;
    ei->disk_inode.i_ctime       = inode->ctime;
    ei->disk_inode.i_blocks      = inode->blocks;
    ei->disk_inode.i_flags       = inode->flags;

    return ext2_write_inode_raw(fs->dev, fs, inode->ino, &ei->disk_inode);
}

/*
 * Delete an inode: free all data blocks, clear the on-disk entry,
 * and release the inode number in the bitmap.
 */
int ext2_delete_inode(inode_t *inode)
{
    if (!inode || !inode->sb || !inode->fs_data) return -1;

    ext2_fs_data_t    *fs = (ext2_fs_data_t *)inode->sb->fs_data;
    ext2_inode_data_t *ei = (ext2_inode_data_t *)inode->fs_data;

    /* Free direct data blocks */
    for (int i = 0; i < 12; i++) {
        if (ei->disk_inode.i_block[i]) {
            ext2_free_block(inode->sb, ei->disk_inode.i_block[i]);
            ei->disk_inode.i_block[i] = 0;
        }
    }

    /* Free singly-indirect block and its children */
    if (ei->disk_inode.i_block[12]) {
        uint32_t ptrs = fs->block_size / sizeof(uint32_t);
        uint32_t *ind = (uint32_t *)kmalloc(fs->block_size);
        if (ind) {
            ext2_read_block(fs->dev, ei->disk_inode.i_block[12], ind);
            for (uint32_t j = 0; j < ptrs; j++) {
                if (ind[j]) ext2_free_block(inode->sb, ind[j]);
            }
            kfree((uintptr_t)ind);
        }
        ext2_free_block(inode->sb, ei->disk_inode.i_block[12]);
        ei->disk_inode.i_block[12] = 0;
    }

    /* Clear and write back the inode */
    memset(&ei->disk_inode, 0, sizeof(ext2_inode_t));
    ext2_write_inode_raw(fs->dev, fs, inode->ino, &ei->disk_inode);

    /* Release the inode number */
    ext2_free_inode(inode->sb, inode->ino);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Inode bitmap allocation / deallocation                            */
/* ------------------------------------------------------------------ */

/*
 * Find and allocate a free inode number.  Scans every group's inode
 * bitmap until a zero bit is found, sets it, and returns the
 * one-based inode number.
 */
ino_t ext2_alloc_inode(superblock_t *sb)
{
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;
    if (fs->sb->s_free_inodes_count == 0) return 0;

    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return 0;

    for (uint32_t g = 0; g < fs->groups_count; g++) {
        if (fs->group_desc[g].bg_free_inodes_count == 0) continue;

        ext2_read_block(fs->dev,
                        fs->group_desc[g].bg_inode_bitmap, bitmap);

        for (uint32_t i = 0; i < fs->sb->s_inodes_per_group; i++) {
            if (!(bitmap[i / 8] & (1 << (i % 8)))) {
                bitmap[i / 8] |= (1 << (i % 8));
                ext2_write_block(fs->dev,
                                 fs->group_desc[g].bg_inode_bitmap, bitmap);
                kfree((uintptr_t)bitmap);

                fs->group_desc[g].bg_free_inodes_count--;
                fs->sb->s_free_inodes_count--;
                ext2_flush_group_desc(sb, g);
                ext2_flush_superblock(sb);

                return g * fs->sb->s_inodes_per_group + i + 1;
            }
        }
    }

    kfree((uintptr_t)bitmap);
    return 0;
}

/*
 * Free an inode number: clear the bitmap bit and bump the free
 * counters in the group descriptor and superblock.
 */
void ext2_free_inode(superblock_t *sb, ino_t ino)
{
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;

    uint32_t group = (ino - 1) / fs->sb->s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb->s_inodes_per_group;

    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return;
    ext2_read_block(fs->dev,
                    fs->group_desc[group].bg_inode_bitmap, bitmap);

    bitmap[index / 8] &= ~(1 << (index % 8));
    ext2_write_block(fs->dev,
                     fs->group_desc[group].bg_inode_bitmap, bitmap);
    kfree((uintptr_t)bitmap);

    fs->group_desc[group].bg_free_inodes_count++;
    fs->sb->s_free_inodes_count++;
    ext2_flush_group_desc(sb, group);
    ext2_flush_superblock(sb);
}
