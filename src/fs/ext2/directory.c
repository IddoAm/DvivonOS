/*
 * ext2/directory.c – Directory entry parsing, lookup, creation,
 *                    and removal for ext2 directories.
 *
 * An ext2 directory is a flat sequence of variable-length
 * ext2_dir_entry_t records inside the directory's data blocks.
 * rec_len links entries together; unused space is absorbed by
 * extending the preceding entry's rec_len.
 */

#include <fs/ext2/ext2.h>
#include <fs/ext2/directory.h>
#include <fs/ext2/block.h>
#include <fs/ext2/inode.h>
#include <fs/ext2/superblock.h>
#include <fs/vfs/inode.h>
#include <fs/vfs/superblock.h>
#include <drivers/block_device.h>
#include <kernel/heap-allocator.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ */
/*  Type conversion helpers                                           */
/* ------------------------------------------------------------------ */

/* Map EXT2_FT_* -> FS_FILE_TYPE_* */
uint32_t ext2_get_dir_entry_type(uint8_t ext2_type)
{
    switch (ext2_type) {
    case EXT2_FT_REG_FILE: return FS_FILE_TYPE_REGULAR;
    case EXT2_FT_DIR:      return FS_FILE_TYPE_DIR;
    case EXT2_FT_CHRDEV:   return FS_FILE_TYPE_CHAR;
    case EXT2_FT_BLKDEV:   return FS_FILE_TYPE_BLOCK;
    case EXT2_FT_FIFO:     return FS_FILE_TYPE_FIFO;
    case EXT2_FT_SOCK:     return FS_FILE_TYPE_SOCKET;
    case EXT2_FT_SYMLINK:  return FS_FILE_TYPE_LINK;
    default:               return 0;
    }
}

/* Map FS_FILE_TYPE_* -> EXT2_FT_* */
uint8_t ext2_set_dir_entry_type(uint32_t vfs_type)
{
    switch (vfs_type & FS_FILE_TYPE_MASK) {
    case FS_FILE_TYPE_REGULAR: return EXT2_FT_REG_FILE;
    case FS_FILE_TYPE_DIR:     return EXT2_FT_DIR;
    case FS_FILE_TYPE_CHAR:    return EXT2_FT_CHRDEV;
    case FS_FILE_TYPE_BLOCK:   return EXT2_FT_BLKDEV;
    case FS_FILE_TYPE_FIFO:    return EXT2_FT_FIFO;
    case FS_FILE_TYPE_SOCKET:  return EXT2_FT_SOCK;
    case FS_FILE_TYPE_LINK:    return EXT2_FT_SYMLINK;
    default:                   return EXT2_FT_UNKNOWN;
    }
}

/* ------------------------------------------------------------------ */
/*  Internal: minimum record length for an entry with name_len bytes  */
/* ------------------------------------------------------------------ */
static uint32_t ext2_dir_rec_len(uint8_t name_len)
{
    /* header (8 bytes) + name, rounded up to 4-byte boundary */
    return (8 + name_len + 3) & ~3U;
}

/* ------------------------------------------------------------------ */
/*  Lookup / find                                                     */
/* ------------------------------------------------------------------ */

/*
 * Search directory data blocks for an entry whose name matches.
 * Returns a heap-allocated copy of the entry on success.
 * Caller must kfree() the returned pointer.
 */
ext2_dir_entry_t *ext2_find_dir_entry(inode_t *dir, const char *name)
{
    if (!dir || !name || !FS_IS_DIR(dir->mode))
        return NULL;

    ext2_fs_data_t    *fs = (ext2_fs_data_t *)dir->sb->fs_data;
    ext2_inode_data_t *ei = (ext2_inode_data_t *)dir->fs_data;
    uint32_t bs   = fs->block_size;
    uint32_t nlen = strlen(name);
    uint32_t dir_blocks = (dir->size + bs - 1) / bs;

    uint8_t *block_buf = (uint8_t *)kmalloc(bs);
    if (!block_buf) return NULL;

    for (uint32_t b = 0; b < dir_blocks; b++) {
        uint32_t phys = ext2_resolve_block_num(
            fs->dev, &ei->disk_inode, b, bs);
        if (phys == 0) continue;
        ext2_read_block(fs->dev, phys, block_buf);

        uint32_t off = 0;
        while (off < bs) {
            ext2_dir_entry_t *de = (ext2_dir_entry_t *)(block_buf + off);
            if (de->rec_len == 0) break;

            if (de->inode != 0 &&
                de->name_len == nlen &&
                memcmp(de->name, name, nlen) == 0) {
                /* Copy the entry onto the heap */
                uint32_t total = ext2_dir_rec_len(de->name_len);
                ext2_dir_entry_t *copy =
                    (ext2_dir_entry_t *)kmalloc(total);
                if (copy)
                    memcpy(copy, de, total);
                kfree((uintptr_t)block_buf);
                return copy;
            }
            off += de->rec_len;
        }
    }

    kfree((uintptr_t)block_buf);
    return NULL;
}

/*
 * VFS lookup: resolve a single path component inside a directory
 * and return the corresponding VFS inode.
 */
inode_t *ext2_lookup(inode_t *dir, const char *name)
{
    ext2_dir_entry_t *de = ext2_find_dir_entry(dir, name);
    if (!de) return NULL;

    ino_t ino = de->inode;
    kfree((uintptr_t)de);
    return ext2_read_inode(dir->sb, ino);
}

/* ------------------------------------------------------------------ */
/*  Add / remove directory entries                                    */
/* ------------------------------------------------------------------ */

/*
 * Append or insert a directory entry.  Walks each block looking for
 * an entry whose rec_len has enough slack to fit the new entry.
 * If no room is found in existing blocks, allocates a new block.
 */
int ext2_add_dir_entry(inode_t *dir, const char *name,
                       ino_t ino, uint8_t type)
{
    if (!dir || !name || ino == 0) return -1;

    ext2_fs_data_t    *fs = (ext2_fs_data_t *)dir->sb->fs_data;
    ext2_inode_data_t *dei = (ext2_inode_data_t *)dir->fs_data;
    uint32_t bs   = fs->block_size;
    uint8_t  nlen = (uint8_t)strlen(name);
    uint32_t need = ext2_dir_rec_len(nlen);
    uint32_t dir_blocks = (dir->size + bs - 1) / bs;

    uint8_t *block_buf = (uint8_t *)kmalloc(bs);
    if (!block_buf) return -1;

    /* Try to find room in existing blocks */
    for (uint32_t b = 0; b < dir_blocks; b++) {
        uint32_t phys = ext2_resolve_block_num(
            fs->dev, &dei->disk_inode, b, bs);
        if (phys == 0) continue;
        ext2_read_block(fs->dev, phys, block_buf);

        uint32_t off = 0;
        while (off < bs) {
            ext2_dir_entry_t *de = (ext2_dir_entry_t *)(block_buf + off);
            if (de->rec_len == 0) break;

            uint32_t actual = ext2_dir_rec_len(de->name_len);
            uint32_t slack  = de->rec_len - actual;

            if (de->inode == 0 && de->rec_len >= need) {
                /* Reuse a deleted entry */
                de->inode    = ino;
                de->name_len = nlen;
                de->file_type = type;
                memcpy(de->name, name, nlen);
                ext2_write_block(fs->dev, phys, block_buf);
                kfree((uintptr_t)block_buf);
                return 0;
            }

            if (slack >= need) {
                /* Shrink current entry and insert new one after it */
                de->rec_len = (uint16_t)actual;
                off += actual;

                ext2_dir_entry_t *new_de =
                    (ext2_dir_entry_t *)(block_buf + off);
                new_de->inode     = ino;
                new_de->rec_len   = (uint16_t)slack;
                new_de->name_len  = nlen;
                new_de->file_type = type;
                memcpy(new_de->name, name, nlen);

                ext2_write_block(fs->dev, phys, block_buf);
                kfree((uintptr_t)block_buf);
                return 0;
            }
            off += de->rec_len;
        }
    }

    /* No room – allocate a new block for the directory */
    block_t new_blk = ext2_alloc_block(dir->sb);
    if (new_blk == 0) { kfree((uintptr_t)block_buf); return -1; }

    if (ext2_assign_block_num(dir->sb, fs->dev, &dei->disk_inode,
                              dir_blocks, new_blk, bs) != 0) {
        ext2_free_block(dir->sb, new_blk);
        kfree((uintptr_t)block_buf);
        return -1;
    }

    memset(block_buf, 0, bs);
    ext2_dir_entry_t *de = (ext2_dir_entry_t *)block_buf;
    de->inode     = ino;
    de->rec_len   = (uint16_t)bs;      /* spans the whole block */
    de->name_len  = nlen;
    de->file_type = type;
    memcpy(de->name, name, nlen);

    ext2_write_block(fs->dev, new_blk, block_buf);
    kfree((uintptr_t)block_buf);

    dir->size += bs;
    dei->disk_inode.i_size = dir->size;
    dei->disk_inode.i_blocks += bs / 512;
    dir->blocks = dei->disk_inode.i_blocks;
    ext2_write_inode(dir);
    return 0;
}

/*
 * Remove a directory entry by name.  The entry's inode field is
 * zeroed and its space is merged into the preceding entry's rec_len.
 */
int ext2_remove_dir_entry(inode_t *dir, const char *name)
{
    if (!dir || !name) return -1;

    ext2_fs_data_t    *fs  = (ext2_fs_data_t *)dir->sb->fs_data;
    ext2_inode_data_t *dei = (ext2_inode_data_t *)dir->fs_data;
    uint32_t bs   = fs->block_size;
    uint32_t nlen = strlen(name);
    uint32_t dir_blocks = (dir->size + bs - 1) / bs;

    uint8_t *block_buf = (uint8_t *)kmalloc(bs);
    if (!block_buf) return -1;

    for (uint32_t b = 0; b < dir_blocks; b++) {
        uint32_t phys = ext2_resolve_block_num(
            fs->dev, &dei->disk_inode, b, bs);
        if (phys == 0) continue;
        ext2_read_block(fs->dev, phys, block_buf);

        uint32_t off = 0;
        ext2_dir_entry_t *prev = NULL;

        while (off < bs) {
            ext2_dir_entry_t *de = (ext2_dir_entry_t *)(block_buf + off);
            if (de->rec_len == 0) break;

            if (de->inode != 0 &&
                de->name_len == nlen &&
                memcmp(de->name, name, nlen) == 0) {
                if (prev) {
                    /* Merge this entry into the preceding one */
                    prev->rec_len += de->rec_len;
                } else {
                    /* First entry in block – just zero the inode */
                    de->inode = 0;
                }
                ext2_write_block(fs->dev, phys, block_buf);
                kfree((uintptr_t)block_buf);
                return 0;
            }
            prev = de;
            off += de->rec_len;
        }
    }

    kfree((uintptr_t)block_buf);
    return -1;   /* not found */
}

/* ------------------------------------------------------------------ */
/*  readdir                                                           */
/* ------------------------------------------------------------------ */

/*
 * Copy raw directory entries from on-disk blocks into a user buffer.
 * offset is a byte position into the linearized directory stream.
 * Returns the number of bytes placed in buf.
 */
int ext2_readdir(inode_t *dir, void *buf, size_t size, uint32_t offset)
{
    if (!dir || !buf || !FS_IS_DIR(dir->mode)) return -1;

    ext2_fs_data_t    *fs  = (ext2_fs_data_t *)dir->sb->fs_data;
    ext2_inode_data_t *dei = (ext2_inode_data_t *)dir->fs_data;
    uint32_t bs = fs->block_size;

    if (offset >= dir->size) return 0;
    if (offset + size > dir->size) size = dir->size - offset;
    if (size == 0) return 0;

    uint8_t *block_buf = (uint8_t *)kmalloc(bs);
    if (!block_buf) return -1;

    uint32_t copied = 0;
    while (copied < size) {
        uint32_t pos        = offset + copied;
        uint32_t blk_idx    = pos / bs;
        uint32_t off_in_blk = pos % bs;
        uint32_t remain     = size - copied;
        uint32_t avail      = bs - off_in_blk;
        if (avail > remain) avail = remain;

        uint32_t phys = ext2_resolve_block_num(
            fs->dev, &dei->disk_inode, blk_idx, bs);
        if (phys == 0) {
            memset((uint8_t *)buf + copied, 0, avail);
        } else {
            ext2_read_block(fs->dev, phys, block_buf);
            memcpy((uint8_t *)buf + copied,
                   block_buf + off_in_blk, avail);
        }
        copied += avail;
    }

    kfree((uintptr_t)block_buf);
    return (int)copied;
}

/* ------------------------------------------------------------------ */
/*  High-level create / mkdir / unlink / rmdir                        */
/* ------------------------------------------------------------------ */

/*
 * Create a regular file inside dir.
 * Allocates a new inode, initializes it, and adds a directory entry.
 */
int ext2_create(inode_t *dir, const char *name, uint32_t mode)
{
    if (!dir || !name) return -1;

    ino_t new_ino = ext2_alloc_inode(dir->sb);
    if (new_ino == 0) return -1;

    ext2_fs_data_t *fs = (ext2_fs_data_t *)dir->sb->fs_data;

    /* Initialize the on-disk inode */
    ext2_inode_t raw;
    memset(&raw, 0, sizeof(raw));
    raw.i_mode        = (uint16_t)(FS_FILE_TYPE_REGULAR | (mode & 0xFFF));
    raw.i_links_count = 1;

    ext2_write_inode_raw(fs->dev, fs, new_ino, &raw);

    /* Add directory entry in parent */
    if (ext2_add_dir_entry(dir, name, new_ino, EXT2_FT_REG_FILE) != 0) {
        ext2_free_inode(dir->sb, new_ino);
        return -1;
    }

    return 0;
}

/*
 * Create a sub-directory inside dir.
 * Allocates a new inode, gives it a data block containing . and ..,
 * and adds an entry in the parent.
 */
int ext2_mkdir(inode_t *dir, const char *name, uint32_t mode)
{
    if (!dir || !name) return -1;

    ext2_fs_data_t *fs = (ext2_fs_data_t *)dir->sb->fs_data;
    uint32_t bs = fs->block_size;

    ino_t new_ino = ext2_alloc_inode(dir->sb);
    if (new_ino == 0) return -1;

    block_t blk = ext2_alloc_block(dir->sb);
    if (blk == 0) {
        ext2_free_inode(dir->sb, new_ino);
        return -1;
    }

    /* Build the initial directory block (. and ..) */
    uint8_t *block_buf = (uint8_t *)kmalloc(bs);
    if (!block_buf) {
        ext2_free_block(dir->sb, blk);
        ext2_free_inode(dir->sb, new_ino);
        return -1;
    }
    memset(block_buf, 0, bs);

    /* "." entry */
    ext2_dir_entry_t *dot = (ext2_dir_entry_t *)block_buf;
    dot->inode     = new_ino;
    dot->rec_len   = 12;
    dot->name_len  = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0]   = '.';

    /* ".." entry – takes the rest of the block */
    ext2_dir_entry_t *dotdot =
        (ext2_dir_entry_t *)(block_buf + dot->rec_len);
    dotdot->inode     = dir->ino;
    dotdot->rec_len   = (uint16_t)(bs - dot->rec_len);
    dotdot->name_len  = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0]   = '.';
    dotdot->name[1]   = '.';

    ext2_write_block(fs->dev, blk, block_buf);
    kfree((uintptr_t)block_buf);

    /* Write the new directory's inode */
    ext2_inode_t raw;
    memset(&raw, 0, sizeof(raw));
    raw.i_mode        = (uint16_t)(FS_FILE_TYPE_DIR | (mode & 0xFFF));
    raw.i_size        = bs;
    raw.i_links_count = 2;          /* . and parent's entry */
    raw.i_blocks      = bs / 512;
    raw.i_block[0]    = blk;

    ext2_write_inode_raw(fs->dev, fs, new_ino, &raw);

    /* Add entry in the parent directory */
    if (ext2_add_dir_entry(dir, name, new_ino, EXT2_FT_DIR) != 0) {
        ext2_free_block(dir->sb, blk);
        ext2_free_inode(dir->sb, new_ino);
        return -1;
    }

    /* Bump parent's link count (for the .. back-link) */
    dir->nlink++;
    ext2_write_inode(dir);

    /* Update directory-count in the group descriptor */
    uint32_t group = (new_ino - 1) / fs->sb->s_inodes_per_group;
    fs->group_desc[group].bg_used_dirs_count++;
    ext2_flush_group_desc(dir->sb, group);

    return 0;
}

/*
 * Remove a file entry and decrement the inode's link count.
 * If link count reaches 0, the inode and its blocks are freed.
 */
int ext2_unlink(inode_t *dir, const char *name)
{
    if (!dir || !name) return -1;

    /* Locate the target inode */
    inode_t *target = ext2_lookup(dir, name);
    if (!target) return -1;
    if (FS_IS_DIR(target->mode)) {
        inode_put(target);
        return -1;   /* use rmdir for directories */
    }

    if (ext2_remove_dir_entry(dir, name) != 0) {
        inode_put(target);
        return -1;
    }

    target->nlink--;
    if (target->nlink == 0) {
        ext2_delete_inode(target);
    } else {
        ext2_write_inode(target);
    }
    inode_put(target);
    return 0;
}

/*
 * Remove a directory.  Fails if the directory is not empty
 * (contains entries other than . and ..).
 */
int ext2_rmdir(inode_t *dir, const char *name)
{
    if (!dir || !name) return -1;

    inode_t *target = ext2_lookup(dir, name);
    if (!target) return -1;
    if (!FS_IS_DIR(target->mode)) {
        inode_put(target);
        return -1;
    }

    ext2_fs_data_t    *fs  = (ext2_fs_data_t *)dir->sb->fs_data;
    ext2_inode_data_t *tei = (ext2_inode_data_t *)target->fs_data;
    uint32_t bs = fs->block_size;

    /* Check for emptiness: only . and .. allowed */
    uint8_t *block_buf = (uint8_t *)kmalloc(bs);
    if (!block_buf) { inode_put(target); return -1; }

    uint32_t blks = (target->size + bs - 1) / bs;
    int entry_count = 0;
    for (uint32_t b = 0; b < blks; b++) {
        uint32_t phys = ext2_resolve_block_num(
            fs->dev, &tei->disk_inode, b, bs);
        if (phys == 0) continue;
        ext2_read_block(fs->dev, phys, block_buf);

        uint32_t off = 0;
        while (off < bs) {
            ext2_dir_entry_t *de =
                (ext2_dir_entry_t *)(block_buf + off);
            if (de->rec_len == 0) break;
            if (de->inode != 0) entry_count++;
            off += de->rec_len;
        }
    }
    kfree((uintptr_t)block_buf);

    if (entry_count > 2) {   /* more than . and .. */
        inode_put(target);
        return -1;
    }

    if (ext2_remove_dir_entry(dir, name) != 0) {
        inode_put(target);
        return -1;
    }

    /* Decrement parent's link count (remove the .. backlink) */
    dir->nlink--;
    ext2_write_inode(dir);

    /* Delete the directory inode */
    ext2_delete_inode(target);

    /* Update group directory count */
    uint32_t group = (target->ino - 1) / fs->sb->s_inodes_per_group;
    if (fs->group_desc[group].bg_used_dirs_count > 0)
        fs->group_desc[group].bg_used_dirs_count--;
    ext2_flush_group_desc(dir->sb, group);

    inode_put(target);
    return 0;
}
