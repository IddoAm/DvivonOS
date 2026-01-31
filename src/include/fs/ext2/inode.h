#ifndef FS_EXT2_INODE_H
#define FS_EXT2_INODE_H

#include <stdint.h>
#include "fs/types.h"
#include "fs/ext2/ext2.h"

/* Function Declarations */

/**
 * ext2_read_inode - Read inode from disk
 * @sb: Superblock
 * @ino: Inode number
 *
 * Loads an inode from disk and creates VFS inode structure.
 * Returns inode pointer on success, NULL on failure.
 */
inode_t *ext2_read_inode(superblock_t *sb, ino_t ino);

/**
 * ext2_write_inode - Write inode to disk
 * @inode: VFS inode to write
 *
 * Saves inode data back to disk.
 * Returns 0 on success, error code on failure.
 */
int ext2_write_inode(inode_t *inode);

/**
 * ext2_delete_inode - Delete inode and free blocks
 * @inode: VFS inode to delete
 *
 * Removes inode from disk and frees all associated blocks.
 * Returns 0 on success, error code on failure.
 */
int ext2_delete_inode(inode_t *inode);

/**
 * ext2_alloc_inode - Allocate a free inode
 * @sb: Superblock
 *
 * Finds and allocates a free inode number.
 * Returns inode number on success, 0 on failure.
 */
ino_t ext2_alloc_inode(superblock_t *sb);

/**
 * ext2_free_inode - Free an inode
 * @sb: Superblock
 * @ino: Inode number to free
 *
 * Marks an inode as free and updates counters.
 */
void ext2_free_inode(superblock_t *sb, ino_t ino);

/**
 * ext2_get_inode_block - Get block number containing inode
 * @fs: Ext2 filesystem data
 * @ino: Inode number
 *
 * Calculates which block contains the specified inode.
 * Returns block number.
 */
uint32_t ext2_get_inode_block(const ext2_fs_data_t *fs, ino_t ino);

/**
 * ext2_read_inode_raw - Read raw inode data from disk
 * @dev: Block device
 * @fs: Ext2 filesystem data
 * @ino: Inode number
 * @inode: Buffer to store inode data
 *
 * Reads raw inode structure from disk without VFS wrapper.
 * Returns 0 on success, error code on failure.
 */
int ext2_read_inode_raw(block_device_t *dev, const ext2_fs_data_t *fs,
                       ino_t ino, ext2_inode_t *inode);

/**
 * ext2_write_inode_raw - Write raw inode data to disk
 * @dev: Block device
 * @fs: Ext2 filesystem data
 * @ino: Inode number
 * @inode: Inode data to write
 *
 * Writes raw inode structure to disk.
 * Returns 0 on success, error code on failure.
 */
int ext2_write_inode_raw(block_device_t *dev, const ext2_fs_data_t *fs,
                        ino_t ino, const ext2_inode_t *inode);

#endif /* FS_EXT2_INODE_H */