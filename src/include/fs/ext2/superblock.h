#ifndef FS_EXT2_SUPERBLOCK_H
#define FS_EXT2_SUPERBLOCK_H

#include <stdint.h>
#include "fs/types.h"
#include "fs/ext2/ext2.h"

/* Function Declarations */

/**
 * ext2_mount - Mount an Ext2 filesystem
 * @dev: Block device containing the filesystem
 *
 * Mounts an Ext2 filesystem and returns superblock.
 * Returns superblock pointer on success, NULL on failure.
 */
superblock_t *ext2_mount(block_device_t *dev);

/**
 * ext2_fill_super - Fill superblock structure for Ext2
 * @sb: Superblock to fill
 * @dev: Block device
 *
 * Parses Ext2 superblock and sets up VFS superblock.
 * Returns 0 on success, error code on failure.
 */
int ext2_fill_super(superblock_t *sb, block_device_t *dev);

/**
 * ext2_read_superblock - Read Ext2 superblock from disk
 * @dev: Block device
 * @sb: Buffer to store superblock
 *
 * Reads the Ext2 superblock from its standard location.
 * Returns 0 on success, error code on failure.
 */
int ext2_read_superblock(block_device_t *dev, ext2_superblock_t *sb);

/**
 * ext2_validate_superblock - Validate Ext2 superblock
 * @sb: Superblock to validate
 *
 * Checks magic number, version, and supported features.
 * Returns 0 if valid, error code if invalid.
 */
int ext2_validate_superblock(const ext2_superblock_t *sb);

/**
 * ext2_read_group_desc - Read block group descriptors
 * @dev: Block device
 * @sb: Ext2 superblock
 * @groups: Array to store group descriptors
 * @count: Number of groups
 *
 * Reads all block group descriptors from disk.
 * Returns 0 on success, error code on failure.
 */
int ext2_read_group_desc(block_device_t *dev, const ext2_superblock_t *sb,
                        ext2_group_desc_t *groups, uint32_t count);

/**
 * ext2_get_block_group - Get block group for inode/block
 * @sb: Ext2 superblock
 * @inode: Inode number (or 0 for block)
 * @block: Block number (ignored if inode != 0)
 *
 * Calculates which block group contains the inode or block.
 * Returns block group number.
 */
uint32_t ext2_get_block_group(const ext2_superblock_t *sb, ino_t inode, block_t block);

/**
 * ext2_put_superblock - Clean up Ext2 superblock
 * @sb: Superblock to clean up
 *
 * Frees Ext2-specific data and resources.
 */
void ext2_put_superblock(superblock_t *sb);

/**
 * ext2_sync - Sync filesystem to disk
 * @sb: Superblock
 *
 * Flushes all dirty data to disk.
 * Returns 0 on success, error code on failure.
 */
int ext2_sync(superblock_t *sb);

/**
 * ext2_statfs - Get filesystem statistics
 * @sb: Superblock
 * @stat: Buffer to store statistics
 *
 * Fills filesystem statistics structure.
 * Returns 0 on success, error code on failure.
 */
int ext2_statfs(superblock_t *sb, void *stat);

#endif /* FS_EXT2_SUPERBLOCK_H */