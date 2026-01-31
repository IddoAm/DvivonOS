#ifndef FS_EXT2_BLOCK_H
#define FS_EXT2_BLOCK_H

#include <stdint.h>
#include "fs/types.h"
#include "fs/ext2/ext2.h"

/* Function Declarations */

/**
 * ext2_alloc_block - Allocate a free block
 * @sb: Superblock
 *
 * Allocates a free block from the filesystem.
 * Returns block number on success, 0 on failure.
 */
block_t ext2_alloc_block(superblock_t *sb);

/**
 * ext2_free_block - Free a block
 * @sb: Superblock
 * @block: Block number to free
 *
 * Marks a block as free in the bitmap and updates counters.
 */
void ext2_free_block(superblock_t *sb, block_t block);

/**
 * ext2_read_block - Read block from disk
 * @dev: Block device
 * @block: Filesystem block number
 * @buf: Buffer to store data
 *
 * Reads one filesystem block from disk.
 * Returns 0 on success, error code on failure.
 */
int ext2_read_block(block_device_t *dev, block_t block, void *buf);

/**
 * ext2_write_block - Write block to disk
 * @dev: Block device
 * @block: Filesystem block number
 * @buf: Buffer containing data
 *
 * Writes one filesystem block to disk.
 * Returns 0 on success, error code on failure.
 */
int ext2_write_block(block_device_t *dev, block_t block, const void *buf);

/**
 * ext2_read_blocks - Read multiple blocks
 * @dev: Block device
 * @start: Starting block number
 * @count: Number of blocks to read
 * @buf: Buffer to store data
 *
 * Reads multiple consecutive blocks from disk.
 * Returns 0 on success, error code on failure.
 */
int ext2_read_blocks(block_device_t *dev, block_t start, uint32_t count, void *buf);

/**
 * ext2_write_blocks - Write multiple blocks
 * @dev: Block device
 * @start: Starting block number
 * @count: Number of blocks to write
 * @buf: Buffer containing data
 *
 * Writes multiple consecutive blocks to disk.
 * Returns 0 on success, error code on failure.
 */
int ext2_write_blocks(block_device_t *dev, block_t start, uint32_t count, const void *buf);

/**
 * ext2_get_block_ptr - Get block pointer from inode
 * @inode: Ext2 inode
 * @block_index: Block index (0-14)
 *
 * Returns the block number for the specified block index in the inode.
 * Handles direct, indirect, double indirect, and triple indirect blocks.
 */
uint32_t ext2_get_block_ptr(const ext2_inode_t *inode, uint32_t block_index);

/**
 * ext2_set_block_ptr - Set block pointer in inode
 * @inode: Ext2 inode
 * @block_index: Block index (0-14)
 * @block: Block number to set
 *
 * Sets the block pointer for the specified block index in the inode.
 */
void ext2_set_block_ptr(ext2_inode_t *inode, uint32_t block_index, uint32_t block);

/**
 * ext2_alloc_indirect_block - Allocate indirect block
 * @sb: Superblock
 *
 * Allocates a block to use as an indirect block and initializes it to zeros.
 * Returns block number on success, 0 on failure.
 */
uint32_t ext2_alloc_indirect_block(superblock_t *sb);

/**
 * ext2_read_indirect_block - Read indirect block
 * @dev: Block device
 * @block: Indirect block number
 * @buf: Buffer to store pointers
 * @count: Number of pointers to read
 *
 * Reads an indirect block containing block pointers.
 * Returns 0 on success, error code on failure.
 */
int ext2_read_indirect_block(block_device_t *dev, uint32_t block, uint32_t *buf, uint32_t count);

/**
 * ext2_write_indirect_block - Write indirect block
 * @dev: Block device
 * @block: Indirect block number
 * @buf: Buffer containing pointers
 * @count: Number of pointers to write
 *
 * Writes an indirect block containing block pointers.
 * Returns 0 on success, error code on failure.
 */
int ext2_write_indirect_block(block_device_t *dev, uint32_t block, const uint32_t *buf, uint32_t count);

/**
 * ext2_truncate_inode - Truncate inode to new size
 * @sb: Superblock
 * @inode: Ext2 inode to truncate
 * @new_size: New size in bytes
 *
 * Frees blocks beyond the new size and updates inode metadata.
 * Returns 0 on success, error code on failure.
 */
int ext2_truncate_inode(superblock_t *sb, ext2_inode_t *inode, uint32_t new_size);

#endif /* FS_EXT2_BLOCK_H */