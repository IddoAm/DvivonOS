/*
 * ext2/ext2_ops.c – VFS file operations for ext2 (read, write, stat,
 *                   chmod, release) and the ext2_init entry-point that
 *                   registers the "ext2" filesystem type.
 *
 * The operations tables ext2_sb_ops and ext2_file_ops are exported
 * (non-static) so that superblock.c and inode.c can reference them.
 */

#include <fs/ext2/ext2.h>
#include <fs/ext2/inode.h>
#include <fs/ext2/block.h>
#include <fs/ext2/directory.h>
#include <fs/ext2/superblock.h>
#include <fs/vfs/filesystem.h>
#include <fs/vfs/inode.h>
#include <fs/vfs/superblock.h>
#include <drivers/block_device.h>
#include <kernel/heap-allocator.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* Forward declarations for operations defined further down */
static int  ext2_read(inode_t *inode, char *buf, size_t count, uint32_t offset);
static int  ext2_write(inode_t *inode, const char *buf, size_t count, uint32_t offset);
static int  ext2_stat(inode_t *inode, fs_stat_t *stat);
static int  ext2_chmod(inode_t *inode, uint32_t mode);
static void ext2_release(inode_t *inode);

/* ------------------------------------------------------------------ */
/*  Operations tables                                                 */
/* ------------------------------------------------------------------ */

/* Superblock operations – used by the VFS superblock layer. */
const superblock_ops_t ext2_sb_ops = {
    .read_inode     = ext2_read_inode,
    .write_inode    = ext2_write_inode,
    .delete_inode   = ext2_delete_inode,
    .sync           = ext2_sync,
    .statfs         = ext2_statfs,
    .put_superblock = ext2_put_superblock,
};

/* File / inode operations – attached to every ext2 VFS inode. */
const file_ops_t ext2_file_ops = {
    .lookup  = ext2_lookup,
    .mkdir   = ext2_mkdir,
    .rmdir   = ext2_rmdir,
    .create  = ext2_create,
    .unlink  = ext2_unlink,
    .read    = ext2_read,
    .write   = ext2_write,
    .stat    = ext2_stat,
    .chmod   = ext2_chmod,
    .release = ext2_release,
};

/* Filesystem type – handed to register_filesystem(). */
static filesystem_type_t ext2_fs_type = {
    .name  = "ext2",
    .mount = ext2_mount,
    .next  = NULL,
};

/* ------------------------------------------------------------------ */
/*  File read / write                                                 */
/* ------------------------------------------------------------------ */

/*
 * Read up to count bytes from a file starting at offset.
 * Resolves each logical file block through the inode's block map
 * (including indirect blocks) and copies into buf.
 * Returns the number of bytes actually read.
 */
static int ext2_read(inode_t *inode, char *buf, size_t count, uint32_t offset)
{
    if (!inode || !buf || !inode->fs_data || !inode->sb)
        return -1;

    ext2_fs_data_t    *fs = (ext2_fs_data_t *)inode->sb->fs_data;
    ext2_inode_data_t *ei = (ext2_inode_data_t *)inode->fs_data;
    uint32_t bs = fs->block_size;

    /* Clamp to file boundaries */
    if (offset >= inode->size) return 0;
    if (offset + count > inode->size) count = inode->size - offset;
    if (count == 0) return 0;

    uint8_t *block_buf = (uint8_t *)kmalloc(bs);
    if (!block_buf) return -1;

    uint32_t bytes_read = 0;
    while (bytes_read < count) {
        uint32_t pos   = offset + bytes_read;
        uint32_t blk   = pos / bs;
        uint32_t boff  = pos % bs;
        uint32_t chunk = bs - boff;
        if (chunk > count - bytes_read)
            chunk = count - bytes_read;

        uint32_t phys = ext2_resolve_block_num(
            fs->dev, &ei->disk_inode, blk, bs);

        if (phys == 0) {
            memset(buf + bytes_read, 0, chunk);     /* sparse hole */
        } else {
            ext2_read_block(fs->dev, phys, block_buf);
            memcpy(buf + bytes_read, block_buf + boff, chunk);
        }
        bytes_read += chunk;
    }

    kfree((uintptr_t)block_buf);
    return (int)bytes_read;
}

/*
 * Write up to count bytes into a file starting at offset.
 * Allocates new data blocks as the file grows.
 * Returns the number of bytes actually written.
 */
static int ext2_write(inode_t *inode, const char *buf,
                       size_t count, uint32_t offset)
{
    if (!inode || !buf || !inode->fs_data || !inode->sb)
        return -1;

    ext2_fs_data_t    *fs = (ext2_fs_data_t *)inode->sb->fs_data;
    ext2_inode_data_t *ei = (ext2_inode_data_t *)inode->fs_data;
    uint32_t bs = fs->block_size;

    uint8_t *block_buf = (uint8_t *)kmalloc(bs);
    if (!block_buf) return -1;

    size_t bytes_written = 0;
    while (bytes_written < count) {
        uint32_t pos   = offset + bytes_written;
        uint32_t blk   = pos / bs;
        uint32_t boff  = pos % bs;
        uint32_t chunk = bs - boff;
        if (chunk > count - bytes_written)
            chunk = count - bytes_written;

        /* Resolve (or allocate) the physical block */
        uint32_t phys = ext2_resolve_block_num(
            fs->dev, &ei->disk_inode, blk, bs);

        if (phys == 0) {
            phys = ext2_alloc_block(inode->sb);
            if (phys == 0) break;

            if (ext2_assign_block_num(inode->sb, fs->dev,
                                      &ei->disk_inode, blk,
                                      phys, bs) != 0) {
                ext2_free_block(inode->sb, phys);
                break;
            }
            ei->disk_inode.i_blocks += bs / 512;
            inode->blocks = ei->disk_inode.i_blocks;

            /* Zero-fill the new block so partial writes are clean */
            memset(block_buf, 0, bs);
        } else if (boff != 0 || chunk != bs) {
            /* Partial-block write – read first */
            ext2_read_block(fs->dev, phys, block_buf);
        }

        memcpy(block_buf + boff, buf + bytes_written, chunk);
        ext2_write_block(fs->dev, phys, block_buf);
        bytes_written += chunk;
    }

    kfree((uintptr_t)block_buf);

    /* Update file size if we grew */
    uint32_t end = offset + bytes_written;
    if (end > inode->size) {
        inode->size = end;
        ei->disk_inode.i_size = end;
    }

    ext2_write_inode(inode);
    return (int)bytes_written;
}

/* ------------------------------------------------------------------ */
/*  Metadata helpers                                                  */
/* ------------------------------------------------------------------ */

/* Fill a fs_stat_t from the VFS inode. */
static int ext2_stat(inode_t *inode, fs_stat_t *stat)
{
    if (!inode || !stat) return -1;

    stat->ino     = inode->ino;
    stat->mode    = inode->mode;
    stat->size    = inode->size;
    stat->uid     = inode->uid;
    stat->gid     = inode->gid;
    stat->nlink   = inode->nlink;
    stat->atime   = inode->atime;
    stat->mtime   = inode->mtime;
    stat->ctime   = inode->ctime;
    stat->blksize = inode->blksize;
    stat->blocks  = inode->blocks;
    return 0;
}

/* Change the permission bits on an inode. */
static int ext2_chmod(inode_t *inode, uint32_t mode)
{
    if (!inode) return -1;

    /* Preserve file-type bits, update permission bits */
    inode->mode = (inode->mode & FS_FILE_TYPE_MASK) | (mode & 0xFFF);
    return ext2_write_inode(inode);
}

/* Release ext2-specific resources when an inode is freed. */
static void ext2_release(inode_t *inode)
{
    (void)inode;
    /* ext2_inode_data_t is freed by the VFS inode_free() path
       which calls kfree(inode->fs_data). Nothing extra needed. */
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */

/* Register the "ext2" filesystem type with the VFS. */
int ext2_init(void)
{
    int ret = register_filesystem(&ext2_fs_type);
    if (ret == 0)
        printf("[ext2] filesystem type registered\n");
    return ret;
}
