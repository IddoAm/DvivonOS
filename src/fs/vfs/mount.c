/*
 * vfs/mount.c – Mount / unmount logic and the mount_init() boot
 *               entry-point that creates the root filesystem.
 *
 * A simple singly-linked list of mount_t structures tracks every
 * mounted volume.  The very first successful mount becomes the
 * "root mount" returned by get_root_mount().
 */

#include "fs/vfs/mount.h"
#include "fs/vfs/superblock.h"
#include "fs/vfs/filesystem.h"
#include "drivers/block_device.h"
#include "drivers/disk/ata.h"
#include <kernel/heap-allocator.h>
#include <lib/stdio.h>
#include <lib/string.h>

/* Head of the mount list and cached root. */
static mount_t *mount_list = NULL;
static mount_t *root_mount = NULL;

/* ------------------------------------------------------------------ */
/*  Allocation helpers                                                */
/* ------------------------------------------------------------------ */

/* Allocate and zero-initialize a mount_t. */
mount_t *mount_alloc(void)
{
    mount_t *m = (mount_t *)kmalloc(sizeof(mount_t));
    if (m)
        memset(m, 0, sizeof(*m));
    return m;
}

/* Free a mount_t. */
void mount_free(mount_t *mnt)
{
    if (mnt)
        kfree((uintptr_t)mnt);
}

/* ------------------------------------------------------------------ */
/*  List management                                                   */
/* ------------------------------------------------------------------ */

/* Prepend to the mount list. */
void mount_add(mount_t *mnt)
{
    if (!mnt) return;
    mnt->next = mount_list;
    mount_list = mnt;
}

/* Remove from the mount list. */
void mount_remove(mount_t *mnt)
{
    if (!mnt) return;

    mount_t **pp = &mount_list;
    while (*pp) {
        if (*pp == mnt) {
            *pp = mnt->next;
            mnt->next = NULL;
            return;
        }
        pp = &(*pp)->next;
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/*
 * Mount a filesystem.
 * Looks up the filesystem type by name, calls its mount callback,
 * and records the result in the mount list.
 */
mount_t *mount(block_device_t *dev, const char *type, uint32_t flags)
{
    if (!dev || !type) return NULL;

    filesystem_type_t *fs_type = get_filesystem(type);
    if (!fs_type) {
        printf("[mount] unknown filesystem type '%s'\n", type);
        return NULL;
    }
    
    superblock_t *sb = fs_type->mount(dev);
    if (!sb) {
        printf("[mount] filesystem mount failed for '%s'\n", type);
        return NULL;
    }
    
    mount_t *mnt = mount_alloc();
    if (!mnt) {
        superblock_free(sb);
        return NULL;
    }

    mnt->dev   = dev->major;
    mnt->type  = fs_type->name;
    mnt->flags = flags;
    mnt->sb    = sb;
    mnt->next  = NULL;

    mount_add(mnt);

    /* First mount becomes the root */
    if (!root_mount)
        root_mount = mnt;

    return mnt;
}

/*
 * Unmount a filesystem: sync, clean up the superblock, and
 * remove the mount entry from the list.
 */
int umount(mount_t *mnt)
{
    if (!mnt) return -1;

    if (mnt->sb) {
        if (mnt->sb->ops && mnt->sb->ops->sync)
            mnt->sb->ops->sync(mnt->sb);
        superblock_free(mnt->sb);
        mnt->sb = NULL;
    }

    mount_remove(mnt);

    if (root_mount == mnt)
        root_mount = NULL;

    mount_free(mnt);
    return 0;
}

/*
 * Find a mount by device number.
 */
mount_t *get_mount(dev_t dev)
{
    for (mount_t *m = mount_list; m; m = m->next) {
        if (m->dev == dev)
            return m;
    }
    return NULL;
}

/*
 * Return the root (first) mount.
 */
mount_t *get_root_mount(void)
{
    return root_mount;
}

/* ------------------------------------------------------------------ */
/*  Boot-time initialization                                          */
/* ------------------------------------------------------------------ */

/*
 * mount_init – called once at boot (after ata_init + filesystem_init).
 *
 * 1. Creates an ATA-backed block device for the primary-master drive.
 * 2. Mounts it as ext2 to establish the root filesystem.
 */
int mount_init(void)
{
    mount_list = NULL;
    root_mount = NULL;

    printf("[vfs] initializing mount subsystem\n");

    /* Create ATA block device for primary master */
    block_device_t *dev = ata_create_block_device();
    if (!dev) {
        printf("[mount] failed to create ATA block device\n");
        return -1;
    }
    printf("[mount] ATA block device '%s' registered\n", dev->name);

    /* Mount root filesystem as ext2 */
    mount_t *mnt = mount(dev, "ext2", 0);
    if (!mnt) {
        printf("[mount] failed to mount root filesystem\n");
        return -1;
    }

    printf("[mount] root filesystem mounted successfully\n");
    return 0;
}
