#ifndef FS_VFS_MOUNT_H
#define FS_VFS_MOUNT_H

#include <stdint.h>
#include "fs/types.h"

/* Forward declarations */
typedef struct mount mount_t;
typedef struct superblock superblock_t;
typedef struct block_device block_device_t;

/* Mount flags */
#define MS_RDONLY       0x0001  /* Mount read-only */
#define MS_NOSUID       0x0002  /* Ignore suid and sgid bits */
#define MS_NODEV        0x0004  /* Disallow access to device special files */
#define MS_NOEXEC       0x0008  /* Disallow program execution */
#define MS_SYNCHRONOUS  0x0010  /* Writes are synced at once */
#define MS_REMOUNT      0x0020  /* Alter flags of a mounted FS */
#define MS_MANDLOCK     0x0040  /* Allow mandatory locks on an FS */
#define MS_DIRSYNC      0x0080  /* Directory modifications are synchronous */
#define MS_NOATIME      0x0100  /* Do not update access times */
#define MS_NODIRATIME   0x0200  /* Do not update directory access times */
#define MS_BIND         0x0400  /* Bind directory at different place */
#define MS_MOVE         0x0800  /* Move subtree */
#define MS_REC          0x1000  /* Recursive loop */
#define MS_SILENT       0x2000  /* Be quiet */
#define MS_POSIXACL     0x4000  /* VFS does not apply the umask */
#define MS_UNBINDABLE   0x8000  /* Change to unbindable */
#define MS_PRIVATE      0x10000 /* Change to private */
#define MS_SLAVE        0x20000 /* Change to slave */
#define MS_SHARED       0x40000 /* Change to shared */
#define MS_RELATIME     0x80000 /* Update atime relative to mtime/ctime */
#define MS_KERNMOUNT    0x100000 /* This is a kern_mount call */
#define MS_I_VERSION    0x200000 /* Update inode I_version field */
#define MS_STRICTATIME  0x400000 /* Always perform atime updates */
#define MS_LAZYTIME     0x800000 /* Update the on-disk [acm]times lazily */

/* Mount structure */
struct mount {
    dev_t dev;                      /* Device number */
    const char *type;               /* Filesystem type */
    uint32_t flags;                 /* Mount flags */
    superblock_t *sb;               /* Superblock */
    mount_t *next;                  /* Next in mount list */
};

/* Function Declarations */

/**
 * mount - Mount a filesystem
 * @dev: Block device to mount
 * @type: Filesystem type
 * @flags: Mount flags
 */
mount_t *mount(block_device_t *dev, const char *type, uint32_t flags);

/**
 * umount - Unmount a filesystem
 * @mnt: Mount to unmount
 */
int umount(mount_t *mnt);

/**
 * mount_init - Initialize mount system
 */
int mount_init(void);

/**
 * get_mount - Get mount for device
 */
mount_t *get_mount(dev_t dev);

/**
 * get_root_mount - Return the root (first) mount
 *
 * Input:  None
 * Output: Pointer to the root mount_t, or NULL if nothing is mounted.
 */
mount_t *get_root_mount(void);

#endif /* FS_VFS_MOUNT_H */