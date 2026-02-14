/*
 * vfs/file.c – Open-file management.
 *
 * file_open() resolves a path to an inode and wraps it in a file_t.
 * file_read / file_write / file_seek / file_stat operate through
 * the inode's file-operation pointers.
 */

#include <fs/vfs/file.h>
#include <fs/vfs/inode.h>
#include <fs/vfs/dentry.h>
#include <kernel/heap-allocator.h>
#include <lib/stdio.h>
#include <lib/string.h>

/*
 * Open a file at the given absolute path.
 * Performs a path_lookup, allocates a file_t, and attaches the inode.
 */
file_t *file_open(const char *path, uint32_t flags)
{
    if (!path) return NULL;

    inode_t *inode = path_lookup(path);
    if (!inode) return NULL;

    file_t *file = (file_t *)kmalloc(sizeof(file_t));
    if (!file) {
        inode_put(inode);
        return NULL;
    }

    file->inode     = inode;
    file->flags     = flags;
    file->offset    = 0;
    file->ref_count = 1;
    return file;
}

/*
 * Close an open file and release its inode reference.
 */
int file_close(file_t *file)
{
    if (!file) return -1;
    if (file->inode) inode_put(file->inode);
    kfree((uintptr_t)file);
    return 0;
}

/*
 * Read count bytes from the file's current offset into buf.
 * Advances the offset on success.
 */
int file_read(file_t *file, void *buf, size_t count)
{
    if (!file || !file->inode) return -1;

    if (file->inode->f_ops && file->inode->f_ops->read) {
        int ret = file->inode->f_ops->read(
            file->inode, (char *)buf, count, file->offset);
        if (ret > 0)
            file->offset += ret;
        return ret;
    }
    return -1;
}

/*
 * Write count bytes from buf at the file's current offset
 * (or at end-of-file if FILE_FLAG_APPEND is set).
 * Advances the offset on success.
 */
int file_write(file_t *file, const void *buf, size_t count)
{
    if (!file || !file->inode) return -1;

    if (file->flags & FILE_FLAG_APPEND)
        file->offset = file->inode->size;

    if (file->inode->f_ops && file->inode->f_ops->write) {
        int ret = file->inode->f_ops->write(
            file->inode, (const char *)buf, count, file->offset);
        if (ret > 0)
            file->offset += ret;
        return ret;
    }
    return -1;
}

/*
 * Seek to an absolute byte offset within the file.
 */
int file_seek(file_t *file, uint32_t offset)
{
    if (!file) return -1;
    file->offset = offset;
    return (int)offset;
}

/*
 * Fill a fs_stat_t structure with the file's metadata.
 */
int file_stat(file_t *file, fs_stat_t *stat)
{
    if (!file || !file->inode) return -1;

    if (file->inode->f_ops && file->inode->f_ops->stat)
        return file->inode->f_ops->stat(file->inode, stat);
    return -1;
}
