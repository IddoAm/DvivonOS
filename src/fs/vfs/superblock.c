#include <fs/vfs/superblock.h>
#include <fs/vfs/inode.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <kernel/heap-allocator.h>

/**
 * superblock_new - Create a new superblock
 * @dev: Device number
 * @type: Filesystem type (e.g., "ext2")
 *
 * Allocates and initializes a new superblock structure.
 * Returns pointer to new superblock, or NULL on error.
 */
superblock_t *superblock_new(dev_t dev, const char *type)
{
    superblock_t *sb = (superblock_t *)kmalloc(sizeof(superblock_t));
    if (!sb)
        return NULL;

    sb->dev = dev;
    sb->blocksize = 4096;  /* Default block size */
    sb->blocksize_bits = 12;  /* log2(4096) */
    sb->flags = 0;

    /* Copy filesystem type */
    sb->type = (const char *)kmalloc(strlen(type) + 1);
    if (!sb->type) {
        kfree((uintptr_t)sb);
        return NULL;
    }
    strcpy((char *)sb->type, type);
    ((char *)sb->type)[strlen(type)] = '\0';

    sb->root = NULL;
    sb->fs_data = NULL;
    sb->ops = NULL;

    return sb;
}

/**
 * superblock_free - Free superblock memory
 * @sb: Superblock to free
 */
void superblock_free(superblock_t *sb)
{
    if (!sb)
        return;

    /* Call filesystem-specific cleanup if available */
    if (sb->ops && sb->ops->put_superblock)
        sb->ops->put_superblock(sb);

    if (sb->root)
        inode_put(sb->root);

    if (sb->fs_data)
        kfree((uintptr_t)sb->fs_data);

    if (sb->type)
        kfree((uintptr_t)sb->type);

    kfree((uintptr_t)sb);
}
