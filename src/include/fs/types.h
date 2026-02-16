#ifndef FS_TYPES_H
#define FS_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* File system types and constants */

typedef uint32_t sector_t;      /* Disk sector number */
typedef uint32_t block_t;       /* File system block number */
typedef uint32_t ino_t;         /* Inode number */
typedef uint32_t dev_t;         /* Device identifier */

/* File types (from S_IFREG, S_IFDIR, etc.) as POSIX standard */
#define FS_FILE_TYPE_MASK    0170000
#define FS_FILE_TYPE_SOCKET  0140000
#define FS_FILE_TYPE_LINK    0120000
#define FS_FILE_TYPE_BLOCK   0060000
#define FS_FILE_TYPE_DIR     0040000
#define FS_FILE_TYPE_CHAR    0020000
#define FS_FILE_TYPE_FIFO    0010000
#define FS_FILE_TYPE_REGULAR 0100000

#define FS_IS_DIR(mode)      (((mode) & FS_FILE_TYPE_MASK) == FS_FILE_TYPE_DIR)
#define FS_IS_FILE(mode)     (((mode) & FS_FILE_TYPE_MASK) == FS_FILE_TYPE_REGULAR)
#define FS_IS_LINK(mode)     (((mode) & FS_FILE_TYPE_MASK) == FS_FILE_TYPE_LINK)
#define FS_IS_BLOCK(mode)    (((mode) & FS_FILE_TYPE_MASK) == FS_FILE_TYPE_BLOCK)
#define FS_IS_CHAR(mode)     (((mode) & FS_FILE_TYPE_MASK) == FS_FILE_TYPE_CHAR)

/* File permissions */
#define FS_PERM_OWNER_R      0400
#define FS_PERM_OWNER_W      0200
#define FS_PERM_OWNER_X      0100
#define FS_PERM_GROUP_R      0040
#define FS_PERM_GROUP_W      0020
#define FS_PERM_GROUP_X      0010
#define FS_PERM_OTHER_R      0004
#define FS_PERM_OTHER_W      0002
#define FS_PERM_OTHER_X      0001

/* File stat structure */
typedef struct {
    ino_t ino;              /* Inode number */
    uint32_t mode;          /* File type and permissions */
    uint32_t size;          /* File size in bytes */
    uint32_t uid;           /* User ID */
    uint32_t gid;           /* Group ID */
    uint32_t nlink;         /* Number of hard links */
    uint32_t atime;         /* Last access time */
    uint32_t mtime;         /* Last modify time */
    uint32_t ctime;         /* Change time */
    uint32_t blksize;       /* Block size for I/O */
    uint32_t blocks;        /* Number of blocks allocated */
} fs_stat_t;

#endif /* FS_TYPES_H */
