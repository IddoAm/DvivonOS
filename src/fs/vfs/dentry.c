#include "fs/dentry.h"
#include "fs/inode.h"
#include <stdlib.h>
#include <string.h>

/**
 * dentry_new - Create a new directory entry
 * @name: Name component
 * @name_len: Length of name without null terminator
 * @inode: Associated inode (can be NULL for negative cache)
 *
 * Allocates and initializes a new dentry structure.
 * Returns pointer to new dentry, or NULL on error.
 */
dentry_t *dentry_new(const char *name, size_t name_len, inode_t *inode)
{
    dentry_t *dentry = (dentry_t *)malloc(sizeof(dentry_t));
    if (!dentry)
        return NULL;

    char *name_copy = (char *)malloc(name_len + 1);
    if (!name_copy) {
        free(dentry);
        return NULL;
    }

    memcpy(name_copy, name, name_len);
    name_copy[name_len] = '\0';

    dentry->name = name_copy;
    dentry->name_len = name_len;
    dentry->inode = inode;
    dentry->ref_count = 1;
    dentry->parent = NULL;
    dentry->fs_data = NULL;

    if (inode)
        inode_get(inode);

    return dentry;
}

/**
 * dentry_get - Increment dentry reference count
 * @dentry: Dentry to reference
 */
void dentry_get(dentry_t *dentry)
{
    if (dentry)
        dentry->ref_count++;
}

/**
 * dentry_put - Decrement dentry reference count
 * @dentry: Dentry to release
 *
 * Decrements the reference count and frees the dentry if count reaches zero.
 */
void dentry_put(dentry_t *dentry)
{
    if (!dentry)
        return;

    dentry->ref_count--;
    if (dentry->ref_count == 0)
        dentry_free(dentry);
}

/**
 * dentry_free - Free dentry memory
 * @dentry: Dentry to free
 */
void dentry_free(dentry_t *dentry)
{
    if (!dentry)
        return;

    if (dentry->inode)
        inode_put(dentry->inode);

    if (dentry->fs_data)
        free(dentry->fs_data);

    if (dentry->name)
        free((void *)dentry->name);

    free(dentry);
}

/**
 * path_lookup - Look up absolute path
 * @path: Absolute path to look up
 *
 * Looks up an absolute path starting from root directory.
 * Returns pointer to inode, or NULL if not found.
 */
inode_t *path_lookup(const char *path)
{
    /* TODO: Implement absolute path lookup */
    return NULL;
}

/**
 * path_lookup_relative - Look up path relative to a directory
 * @start: Starting directory inode
 * @path: Relative path to look up
 *
 * Looks up a relative path starting from given directory.
 * Returns pointer to inode, or NULL if not found.
 */
inode_t *path_lookup_relative(inode_t *start, const char *path)
{
    /* TODO: Implement relative path lookup */
    return NULL;
}
