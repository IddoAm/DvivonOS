#ifndef FS_FILE_H
#define FS_FILE_H

#include <stdint.h>
#include "fs/types.h"

/* Forward declarations */
typedef struct file file_t;
typedef struct inode inode_t;

/* File flags */
#define FILE_FLAG_READ      0x001
#define FILE_FLAG_WRITE     0x002
#define FILE_FLAG_APPEND    0x004
#define FILE_FLAG_NONBLOCK  0x008

/* File structure - represents an open file */
struct file {
    inode_t *inode;                 /* Pointer to inode */
    uint32_t flags;                 /* Open flags (read/write/append) */
    uint32_t offset;                /* Current file offset */
    uint32_t ref_count;             /* Reference count */
};

/* File operations - open/close/read/write */
file_t *file_open(const char *path, uint32_t flags);
int file_close(file_t *file);
int file_read(file_t *file, void *buf, size_t count);
int file_write(file_t *file, const void *buf, size_t count);
int file_seek(file_t *file, uint32_t offset);
int file_stat(file_t *file, fs_stat_t *stat);

#endif /* FS_FILE_H */
