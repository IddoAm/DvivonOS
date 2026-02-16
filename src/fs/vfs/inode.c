#include <fs/vfs/inode.h>
#include <fs/vfs/superblock.h>
#include <lib/stdio.h>
#include <kernel/heap-allocator.h>

/**
 * inode_new - Create a new inode
 * @sb: Superblock of the inode
 * @ino: Inode number
 *
 * Allocates and initializes a new inode structure.
 * Returns pointer to new inode, or NULL on error.
 */
inode_t *inode_new(superblock_t *sb, ino_t ino)
{
    inode_t *inode = (inode_t *)kmalloc(sizeof(inode_t));
    if (!inode)
        return NULL;

    inode->ino = ino;
    inode->sb = sb;
    inode->mode = 0;
    inode->size = 0;
    inode->uid = 0;
    inode->gid = 0;
    inode->nlink = 0;
    inode->atime = 0;
    inode->mtime = 0;
    inode->ctime = 0;
    inode->blksize = sb->blocksize;
    inode->blocks = 0;
    inode->flags = 0;
    inode->ref_count = 1;
    inode->fs_data = NULL;
    inode->f_ops = NULL;

    return inode;
}

/**
 * inode_get - Increment inode reference count
 * @inode: Inode to reference
 */
void inode_get(inode_t *inode)
{
    if (inode)
        inode->ref_count++;
}

/**
 * inode_put - Decrement inode reference count
 * @inode: Inode to release
 *
 * Decrements the reference count and frees the inode if count reaches zero.
 */
void inode_put(inode_t *inode)
{
    if (!inode)
        return;

    inode->ref_count--;
    if (inode->ref_count == 0)
        inode_free(inode);
}

/**
 * inode_free - Free inode memory
 * @inode: Inode to free
 */
void inode_free(inode_t *inode)
{
    if (!inode)
        return;

    /* Call filesystem-specific release if available */
    if (inode->f_ops && inode->f_ops->release)
        inode->f_ops->release(inode);

    if (inode->fs_data)
        kfree((uintptr_t)inode->fs_data);

    kfree((uintptr_t)inode);
}
