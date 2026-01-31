#ifndef FS_DENTRY_H
#define FS_DENTRY_H

#include <stdint.h>
#include "fs/types.h"

/* Forward declarations */
typedef struct dentry dentry_t;
typedef struct inode inode_t;

/* Directory entry (dentry) - represents a name->inode mapping in the cache */
struct dentry {
    const char *name;               /* Name component */
    size_t name_len;                /* Length of name */
    inode_t *inode;                 /* Pointer to inode (NULL if negative cache) */
    uint32_t ref_count;             /* Reference count */
    dentry_t *parent;               /* Parent directory entry */
    
    /* Filesystem-specific data */
    void *fs_data;                  /* Filesystem-specific dentry data */
};

/* Dentry management functions */
dentry_t *dentry_new(const char *name, size_t name_len, inode_t *inode);
void dentry_get(dentry_t *dentry);
void dentry_put(dentry_t *dentry);
void dentry_free(dentry_t *dentry);

/* Path lookup */
inode_t *path_lookup(const char *path);
inode_t *path_lookup_relative(inode_t *start, const char *path);

#endif /* FS_DENTRY_H */
