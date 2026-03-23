/*
 * vfs/dentry.c – Directory-entry (dentry) management and path lookup.
 *
 * dentry_new / dentry_get / dentry_put / dentry_free manage the
 * in-memory cache entries.
 *
 * path_lookup() and path_lookup_relative() walk a '/' separated
 * path by repeatedly calling the inode's lookup file-operation.
 */

#include <fs/vfs/dentry.h>
#include <fs/vfs/inode.h>
#include <fs/vfs/superblock.h>
#include <fs/vfs/mount.h>
#include <kernel/heap-allocator.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ */
/*  Dentry lifecycle                                                  */
/* ------------------------------------------------------------------ */

/* Allocate a dentry for a given name+inode pair. */
dentry_t *dentry_new(const char *name, size_t name_len, inode_t *inode)
{
    dentry_t *dentry = (dentry_t *)kmalloc(sizeof(dentry_t));
    if (!dentry) return NULL;

    char *name_copy = (char *)kmalloc(name_len + 1);
    if (!name_copy) {
        kfree((uintptr_t)dentry);
        return NULL;
    }
    memcpy(name_copy, name, name_len);
    name_copy[name_len] = '\0';

    dentry->name      = name_copy;
    dentry->name_len  = name_len;
    dentry->inode     = inode;
    dentry->ref_count = 1;
    dentry->parent    = NULL;
    dentry->fs_data   = NULL;

    if (inode) inode_get(inode);
    return dentry;
}

void dentry_get(dentry_t *dentry)
{
    if (dentry) dentry->ref_count++;
}

void dentry_put(dentry_t *dentry)
{
    if (!dentry) return;
    dentry->ref_count--;
    if (dentry->ref_count == 0)
        dentry_free(dentry);
}

void dentry_free(dentry_t *dentry)
{
    if (!dentry) return;
    if (dentry->inode)   inode_put(dentry->inode);
    if (dentry->fs_data) kfree((uintptr_t)dentry->fs_data);
    if (dentry->name)    kfree((uintptr_t)dentry->name);
    kfree((uintptr_t)dentry);
}

/* ------------------------------------------------------------------ */
/*  Path resolution                                                   */
/* ------------------------------------------------------------------ */

/*
 * Walk a relative path (no leading '/') starting from start.
 * Each component between '/' separators is resolved via the
 * current inode's ->lookup() callback.
 *
 * Returns the final inode with its ref-count incremented,
 * or NULL if any component fails to resolve.
 */
inode_t *path_lookup_relative(inode_t *start, const char *path)
{
    if (!start || !path) return NULL;

    inode_t *current = start;
    
    char component[256];
    
    while (*path) {
        inode_get(current);
        /* Skip slashes */
        while (*path == '/') path++;
        if (*path == '\0') break;

        /* Extract next component */
        int i = 0;
        while (*path && *path != '/' && i < 255)
            component[i++] = *path++;
        component[i] = '\0';

        if (!current->f_ops || !current->f_ops->lookup) {
            inode_put(current);
            return NULL;
        }

        inode_t *next = current->f_ops->lookup(current, component);
        inode_put(current);
        if (!next) return NULL;
        current = next;
    }

    return current;
}

/*
 * Resolve an absolute path (must start with '/') from the root
 * of the currently mounted filesystem.
 *
 * Returns the inode at the end of the path with ref-count
 * incremented, or NULL on failure.
 */
inode_t *path_lookup(const char *path)
{
    if (!path || path[0] != '/')
        return NULL;

    mount_t *rmnt = get_root_mount();
    if (!rmnt || !rmnt->sb || !rmnt->sb->root)
        return NULL;

    inode_t *root = rmnt->sb->root;

    /* Skip leading slash(es) */
    path++;
    while (*path == '/') path++;

    /* Empty path after '/' means root directory */
    if (*path == '\0') {
        inode_get(root);
        return root;
    }

    return path_lookup_relative(root, path);
}
