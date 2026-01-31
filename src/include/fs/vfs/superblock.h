#ifndef FS_SUPERBLOCK_H
#define FS_SUPERBLOCK_H

#include <stdint.h>
#include "fs/types.h"

/* Forward declarations */
typedef struct superblock superblock_t;
typedef struct superblock_ops superblock_ops_t;

/* Superblock structure - represents mounted filesystem */
struct superblock {
    dev_t dev;                      /* Device identifier */
    uint32_t blocksize;             /* Filesystem block size */
    uint32_t blocksize_bits;        /* log2(blocksize) */
    uint32_t flags;                 /* Superblock flags */
    const char *type;               /* Filesystem type (e.g., "ext2") */
    
    /* Root directory inode */
    inode_t *root;
    
    /* Filesystem-specific data */
    void *fs_data;                  /* Pointer to filesystem-specific superblock data */
    
    /* Superblock operations */
    const superblock_ops_t *ops;
};

/* Superblock operations - filesystem-specific methods */
typedef struct superblock_ops {
    /* Inode operations */
    inode_t *(*read_inode)(superblock_t *sb, ino_t ino);
    int (*write_inode)(inode_t *inode);
    int (*delete_inode)(inode_t *inode);
    
    /* Filesystem operations */
    int (*sync)(superblock_t *sb);
    int (*statfs)(superblock_t *sb, void *stat);
    
    /* Cleanup */
    void (*put_superblock)(superblock_t *sb);
} superblock_ops_t;

/* Superblock management */
superblock_t *superblock_new(dev_t dev, const char *type);
void superblock_free(superblock_t *sb);

#endif /* FS_SUPERBLOCK_H */
