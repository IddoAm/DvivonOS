#ifndef FS_INODE_H
#define FS_INODE_H

#include <stdint.h>
#include <fs/types.h>

/* Forward declarations */
typedef struct inode inode_t;
typedef struct superblock superblock_t;
typedef struct file_operations file_ops_t;

/* Inode structure - represents a file or directory */
struct inode {
    ino_t ino;                      /* Inode number */
    superblock_t *sb;               /* Parent superblock */
    uint32_t mode;                  /* File type and permissions */
    uint32_t size;                  /* File size in bytes */
    uint32_t uid;                   /* User ID */
    uint32_t gid;                   /* Group ID */
    uint32_t nlink;                 /* Hard link count */
    uint32_t atime;                 /* Last access time */
    uint32_t mtime;                 /* Last modify time */
    uint32_t ctime;                 /* Change time */
    uint32_t blksize;               /* Block size for I/O */
    uint32_t blocks;                /* Number of blocks allocated */
    uint32_t flags;                 /* Inode flags */
    uint32_t ref_count;             /* Reference count for cache */
    
    /* Filesystem-specific data (e.g., ext2 inode data) */
    void *fs_data;                  /* Filesystem-specific inode data */
    
    /* File operations */
    const file_ops_t *f_ops;        /* Pointer to file operations */
};

/* File operations structure - defines how to interact with inode contents */
typedef struct file_operations {
    /* Directory operations */
    inode_t *(*lookup)(inode_t *dir, const char *name);
    int (*mkdir)(inode_t *dir, const char *name, uint32_t mode);
    int (*rmdir)(inode_t *dir, const char *name);
    
    /* File operations */
    int (*create)(inode_t *dir, const char *name, uint32_t mode);
    int (*unlink)(inode_t *dir, const char *name);
    
    /* Read/write operations */
    int (*read)(inode_t *inode, char *buf, size_t count, uint32_t offset);
    int (*write)(inode_t *inode, const char *buf, size_t count, uint32_t offset);
    
    /* Inode metadata operations */
    int (*stat)(inode_t *inode, fs_stat_t *stat);
    int (*chmod)(inode_t *inode, uint32_t mode);
    
    /* Resource cleanup */
    void (*release)(inode_t *inode);
} file_ops_t;

/* Inode management functions */
inode_t *inode_new(superblock_t *sb, ino_t ino);
void inode_get(inode_t *inode);
void inode_put(inode_t *inode);
void inode_free(inode_t *inode);

#endif /* FS_INODE_H */
