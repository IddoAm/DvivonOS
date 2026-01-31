#ifndef FS_VFS_FILESYSTEM_H
#define FS_VFS_FILESYSTEM_H

#include <stdint.h>

/* Forward declarations */
typedef struct filesystem_type filesystem_type_t;
typedef struct superblock superblock_t;
typedef struct block_device block_device_t;

/* Filesystem type structure */
struct filesystem_type {
    const char *name;                           /* Filesystem name */
    superblock_t *(*mount)(block_device_t *dev); /* Mount function */
    filesystem_type_t *next;                    /* Next in list */
};

/* Function Declarations */

/**
 * register_filesystem - Register a filesystem type
 * @fs: Filesystem type to register
 */
int register_filesystem(filesystem_type_t *fs);

/**
 * unregister_filesystem - Unregister a filesystem type
 * @fs: Filesystem type to unregister
 */
int unregister_filesystem(filesystem_type_t *fs);

/**
 * get_filesystem - Get filesystem type by name
 * @name: Filesystem name
 */
filesystem_type_t *get_filesystem(const char *name);

/**
 * filesystem_init - Initialize filesystem registration
 */
int filesystem_init(void);

#endif /* FS_VFS_FILESYSTEM_H */