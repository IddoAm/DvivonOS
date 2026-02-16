/*
 * ext2/block.c – Ext2 block-level I/O, bitmap allocation, and
 *                indirect-block resolution / assignment.
 *
 * Every ext2_read_block / ext2_write_block call delegates to the
 * underlying block_device_t whose block_size has already been set
 * to the ext2 filesystem block size during mount.
 */

#include <fs/ext2/ext2.h>
#include <fs/ext2/block.h>
#include <fs/ext2/superblock.h>
#include <drivers/block_device.h>
#include <kernel/heap-allocator.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ */
/*  Basic block I/O – thin wrappers over the block device ops         */
/* ------------------------------------------------------------------ */

/* Read one filesystem block from disk. */
int ext2_read_block(block_device_t *dev, block_t block, void *buf)
{
    if (!dev || !dev->ops || !dev->ops->read_block || !buf)
        return -1;
    return dev->ops->read_block(dev, block, buf);
}

/* Write one filesystem block to disk. */
int ext2_write_block(block_device_t *dev, block_t block, const void *buf)
{
    if (!dev || !dev->ops || !dev->ops->write_block || !buf)
        return -1;
    return dev->ops->write_block(dev, block, buf);
}

/* Read count consecutive filesystem blocks starting at start. */
int ext2_read_blocks(block_device_t *dev, block_t start,
                     uint32_t count, void *buf)
{
    if (!dev || !dev->ops || !dev->ops->read_blocks || !buf)
        return -1;
    return dev->ops->read_blocks(dev, start, count, buf);
}

/* Write count consecutive filesystem blocks starting at start. */
int ext2_write_blocks(block_device_t *dev, block_t start,
                      uint32_t count, const void *buf)
{
    if (!dev || !dev->ops || !dev->ops->write_blocks || !buf)
        return -1;
    return dev->ops->write_blocks(dev, start, count, buf);
}

/* ------------------------------------------------------------------ */
/*  Direct i_block[] access (indices 0-14)                            */
/* ------------------------------------------------------------------ */

/* Return the raw value stored in inode->i_block[block_index]. */
uint32_t ext2_get_block_ptr(const ext2_inode_t *inode, uint32_t block_index)
{
    if (!inode || block_index > 14)
        return 0;
    return inode->i_block[block_index];
}

/* Set inode->i_block[block_index] to block. */
void ext2_set_block_ptr(ext2_inode_t *inode, uint32_t block_index,
                        uint32_t block)
{
    if (!inode || block_index > 14)
        return;
    inode->i_block[block_index] = block;
}

/* ------------------------------------------------------------------ */
/*  Indirect-block helpers                                            */
/* ------------------------------------------------------------------ */

/* Read an indirect block (array of uint32 pointers). */
int ext2_read_indirect_block(block_device_t *dev, uint32_t block,
                             uint32_t *buf, uint32_t count)
{
    (void)count;
    if (!block || !buf)
        return -1;
    return ext2_read_block(dev, block, buf);
}

/* Write an indirect block. */
int ext2_write_indirect_block(block_device_t *dev, uint32_t block,
                              const uint32_t *buf, uint32_t count)
{
    (void)count;
    if (!block || !buf)
        return -1;
    return ext2_write_block(dev, block, buf);
}

/* Allocate a new zeroed indirect block. */
uint32_t ext2_alloc_indirect_block(superblock_t *sb)
{
    block_t blk = ext2_alloc_block(sb);
    if (blk == 0)
        return 0;

    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;
    uint8_t *zero = (uint8_t *)kmalloc(fs->block_size);
    if (!zero) {
        ext2_free_block(sb, blk);
        return 0;
    }
    memset(zero, 0, fs->block_size);
    ext2_write_block(fs->dev, blk, zero);
    kfree((uintptr_t)zero);
    return blk;
}

/* ------------------------------------------------------------------ */
/*  Logical-to-physical block resolution                              */
/* ------------------------------------------------------------------ */

/*
 * Walk the ext2 direct / singly / doubly / triply indirect block map
 * to translate a logical file-block index into a physical disk block
 * number.  Returns 0 for a hole (sparse region).
 */
uint32_t ext2_resolve_block_num(block_device_t *dev,
                                const ext2_inode_t *inode,
                                uint32_t logical_block,
                                uint32_t block_size)
{
    uint32_t ptrs = block_size / sizeof(uint32_t);
    uint32_t *ind = NULL;
    uint32_t result;

    /* --- direct blocks 0-11 --- */
    if (logical_block < 12)
        return inode->i_block[logical_block];

    logical_block -= 12;

    /* --- singly indirect (i_block[12]) --- */
    if (logical_block < ptrs) {
        if (inode->i_block[12] == 0)
            return 0;
        ind = (uint32_t *)kmalloc(block_size);
        if (!ind) return 0;
        ext2_read_block(dev, inode->i_block[12], ind);
        result = ind[logical_block];
        kfree((uintptr_t)ind);
        return result;
    }
    logical_block -= ptrs;

    /* --- doubly indirect (i_block[13]) --- */
    if (logical_block < ptrs * ptrs) {
        if (inode->i_block[13] == 0)
            return 0;
        ind = (uint32_t *)kmalloc(block_size);
        if (!ind) return 0;
        ext2_read_block(dev, inode->i_block[13], ind);
        uint32_t idx1 = logical_block / ptrs;
        uint32_t idx2 = logical_block % ptrs;
        uint32_t singly_blk = ind[idx1];
        if (singly_blk == 0) { kfree((uintptr_t)ind); return 0; }
        ext2_read_block(dev, singly_blk, ind);
        result = ind[idx2];
        kfree((uintptr_t)ind);
        return result;
    }
    logical_block -= ptrs * ptrs;

    /* --- triply indirect (i_block[14]) --- */
    if (inode->i_block[14] == 0)
        return 0;
    ind = (uint32_t *)kmalloc(block_size);
    if (!ind) return 0;

    ext2_read_block(dev, inode->i_block[14], ind);
    uint32_t i1 = logical_block / (ptrs * ptrs);
    uint32_t rem = logical_block % (ptrs * ptrs);
    uint32_t dbl_blk = ind[i1];
    if (dbl_blk == 0) { kfree((uintptr_t)ind); return 0; }

    ext2_read_block(dev, dbl_blk, ind);
    uint32_t i2 = rem / ptrs;
    uint32_t i3 = rem % ptrs;
    uint32_t sgl_blk = ind[i2];
    if (sgl_blk == 0) { kfree((uintptr_t)ind); return 0; }

    ext2_read_block(dev, sgl_blk, ind);
    result = ind[i3];
    kfree((uintptr_t)ind);
    return result;
}

/* ------------------------------------------------------------------ */
/*  Logical-to-physical block assignment (for writes)                 */
/* ------------------------------------------------------------------ */

/*
 * Store phys_block at the given logical_block position inside the
 * inode's block map, allocating intermediate indirect blocks on
 * the fly.  Returns 0 on success, -1 on failure.
 */
int ext2_assign_block_num(superblock_t *sb, block_device_t *dev,
                          ext2_inode_t *inode, uint32_t logical_block,
                          uint32_t phys_block, uint32_t block_size)
{
    uint32_t ptrs = block_size / sizeof(uint32_t);
    uint32_t *ind = NULL;

    /* --- direct blocks 0-11 --- */
    if (logical_block < 12) {
        inode->i_block[logical_block] = phys_block;
        return 0;
    }
    logical_block -= 12;

    /* --- singly indirect --- */
    if (logical_block < ptrs) {
        if (inode->i_block[12] == 0) {
            inode->i_block[12] = ext2_alloc_indirect_block(sb);
            if (inode->i_block[12] == 0) return -1;
        }
        ind = (uint32_t *)kmalloc(block_size);
        if (!ind) return -1;
        ext2_read_block(dev, inode->i_block[12], ind);
        ind[logical_block] = phys_block;
        ext2_write_block(dev, inode->i_block[12], ind);
        kfree((uintptr_t)ind);
        return 0;
    }
    logical_block -= ptrs;

    /* --- doubly indirect --- */
    if (logical_block < ptrs * ptrs) {
        if (inode->i_block[13] == 0) {
            inode->i_block[13] = ext2_alloc_indirect_block(sb);
            if (inode->i_block[13] == 0) return -1;
        }
        ind = (uint32_t *)kmalloc(block_size);
        if (!ind) return -1;
        ext2_read_block(dev, inode->i_block[13], ind);
        uint32_t idx1 = logical_block / ptrs;
        uint32_t idx2 = logical_block % ptrs;
        if (ind[idx1] == 0) {
            ind[idx1] = ext2_alloc_indirect_block(sb);
            if (ind[idx1] == 0) { kfree((uintptr_t)ind); return -1; }
            ext2_write_block(dev, inode->i_block[13], ind);
        }
        uint32_t singly_blk = ind[idx1];
        ext2_read_block(dev, singly_blk, ind);
        ind[idx2] = phys_block;
        ext2_write_block(dev, singly_blk, ind);
        kfree((uintptr_t)ind);
        return 0;
    }

    /* triply indirect – not implemented for simplicity */
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Block bitmap allocation / deallocation                            */
/* ------------------------------------------------------------------ */

/*
 * Scan every block-group bitmap for a free block.  On success the
 * bit is set, counters are decremented, and the group descriptor +
 * superblock are flushed to disk.
 */
block_t ext2_alloc_block(superblock_t *sb)
{
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;

    if (fs->sb->s_free_blocks_count == 0)
        return 0;

    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return 0;

    for (uint32_t g = 0; g < fs->groups_count; g++) {
        if (fs->group_desc[g].bg_free_blocks_count == 0)
            continue;

        ext2_read_block(fs->dev, fs->group_desc[g].bg_block_bitmap, bitmap);

        uint32_t bits = fs->sb->s_blocks_per_group;
        /* Last group may have fewer blocks */
        if (g == fs->groups_count - 1) {
            uint32_t rem = fs->sb->s_blocks_count -
                           g * fs->sb->s_blocks_per_group -
                           fs->sb->s_first_data_block;
            if (rem < bits) bits = rem;
        }

        for (uint32_t i = 0; i < bits; i++) {
            if (!(bitmap[i / 8] & (1 << (i % 8)))) {
                /* Mark used */
                bitmap[i / 8] |= (1 << (i % 8));
                ext2_write_block(fs->dev,
                                 fs->group_desc[g].bg_block_bitmap, bitmap);
                kfree((uintptr_t)bitmap);

                fs->group_desc[g].bg_free_blocks_count--;
                fs->sb->s_free_blocks_count--;
                ext2_flush_group_desc(sb, g);
                ext2_flush_superblock(sb);

                return fs->sb->s_first_data_block +
                       g * fs->sb->s_blocks_per_group + i;
            }
        }
    }

    kfree((uintptr_t)bitmap);
    return 0;
}

/*
 * Free a previously-allocated block: clear its bitmap bit and bump
 * the free counters in the group descriptor and superblock.
 */
void ext2_free_block(superblock_t *sb, block_t block)
{
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;

    uint32_t rel = block - fs->sb->s_first_data_block;
    uint32_t group = rel / fs->sb->s_blocks_per_group;
    uint32_t index = rel % fs->sb->s_blocks_per_group;

    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return;
    ext2_read_block(fs->dev, fs->group_desc[group].bg_block_bitmap, bitmap);

    bitmap[index / 8] &= ~(1 << (index % 8));
    ext2_write_block(fs->dev,
                     fs->group_desc[group].bg_block_bitmap, bitmap);
    kfree((uintptr_t)bitmap);

    fs->group_desc[group].bg_free_blocks_count++;
    fs->sb->s_free_blocks_count++;
    ext2_flush_group_desc(sb, group);
    ext2_flush_superblock(sb);
}

/* ------------------------------------------------------------------ */
/*  Truncate                                                          */
/* ------------------------------------------------------------------ */

/*
 * Free all data blocks beyond new_size and update i_size / i_blocks.
 * Currently handles direct and singly-indirect blocks.
 */
int ext2_truncate_inode(superblock_t *sb, ext2_inode_t *inode,
                        uint32_t new_size)
{
    ext2_fs_data_t *fs = (ext2_fs_data_t *)sb->fs_data;
    uint32_t bs = fs->block_size;
    uint32_t old_blocks = (inode->i_size + bs - 1) / bs;
    uint32_t new_blocks = (new_size + bs - 1) / bs;

    /* Free direct blocks beyond the new count */
    for (uint32_t i = new_blocks; i < old_blocks && i < 12; i++) {
        if (inode->i_block[i]) {
            ext2_free_block(sb, inode->i_block[i]);
            inode->i_block[i] = 0;
        }
    }

    /* Free singly-indirect entries beyond the new count */
    if (old_blocks > 12 && inode->i_block[12]) {
        uint32_t ptrs = bs / sizeof(uint32_t);
        uint32_t *ind = (uint32_t *)kmalloc(bs);
        if (ind) {
            ext2_read_block(fs->dev, inode->i_block[12], ind);
            uint32_t start = (new_blocks > 12) ? new_blocks - 12 : 0;
            for (uint32_t i = start; i < ptrs; i++) {
                if (ind[i]) {
                    ext2_free_block(sb, ind[i]);
                    ind[i] = 0;
                }
            }
            if (new_blocks <= 12) {
                ext2_free_block(sb, inode->i_block[12]);
                inode->i_block[12] = 0;
            } else {
                ext2_write_block(fs->dev, inode->i_block[12], ind);
            }
            kfree((uintptr_t)ind);
        }
    }

    inode->i_size = new_size;
    /* i_blocks counts 512-byte units on disk */
    inode->i_blocks = new_blocks * (bs / 512);
    return 0;
}
