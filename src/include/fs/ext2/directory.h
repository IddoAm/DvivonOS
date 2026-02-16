#ifndef FS_EXT2_DIRECTORY_H
#define FS_EXT2_DIRECTORY_H

#include <stdint.h>
#include "fs/types.h"
#include "fs/ext2/ext2.h"

/* Function Declarations */

/**
 * ext2_lookup - Look up entry in directory
 * @dir: Directory inode
 * @name: Name to look up
 *
 * Searches for a directory entry with the given name.
 * Returns inode pointer on success, NULL on failure.
 */
inode_t *ext2_lookup(inode_t *dir, const char *name);

/**
 * ext2_mkdir - Create new directory
 * @dir: Parent directory inode
 * @name: New directory name
 * @mode: Directory permissions
 *
 * Creates a new directory with . and .. entries.
 * Returns 0 on success, error code on failure.
 */
int ext2_mkdir(inode_t *dir, const char *name, uint32_t mode);

/**
 * ext2_rmdir - Remove directory
 * @dir: Parent directory inode
 * @name: Directory name to remove
 *
 * Removes an empty directory and updates parent.
 * Returns 0 on success, error code on failure.
 */
int ext2_rmdir(inode_t *dir, const char *name);

/**
 * ext2_create - Create new file
 * @dir: Parent directory inode
 * @name: New file name
 * @mode: File permissions
 *
 * Creates a new regular file inode and directory entry.
 * Returns 0 on success, error code on failure.
 */
int ext2_create(inode_t *dir, const char *name, uint32_t mode);

/**
 * ext2_unlink - Remove file
 * @dir: Parent directory inode
 * @name: File name to remove
 *
 * Removes a file and decrements link count.
 * Returns 0 on success, error code on failure.
 */
int ext2_unlink(inode_t *dir, const char *name);

/**
 * ext2_readdir - Read directory entries
 * @dir: Directory inode
 * @buf: Buffer to store entries
 * @size: Buffer size
 * @offset: Starting offset
 *
 * Reads directory entries into the buffer.
 * Returns number of bytes read, or error code.
 */
int ext2_readdir(inode_t *dir, void *buf, size_t size, uint32_t offset);

/**
 * ext2_add_dir_entry - Add entry to directory
 * @dir: Directory inode
 * @name: Entry name
 * @ino: Inode number
 * @type: File type
 *
 * Adds a new directory entry to the directory.
 * Returns 0 on success, error code on failure.
 */
int ext2_add_dir_entry(inode_t *dir, const char *name, ino_t ino, uint8_t type);

/**
 * ext2_remove_dir_entry - Remove entry from directory
 * @dir: Directory inode
 * @name: Entry name to remove
 *
 * Removes a directory entry and compacts the directory.
 * Returns 0 on success, error code on failure.
 */
int ext2_remove_dir_entry(inode_t *dir, const char *name);

/**
 * ext2_find_dir_entry - Find directory entry by name
 * @dir: Directory inode
 * @name: Entry name to find
 *
 * Searches for a directory entry with the given name.
 * Returns pointer to entry on success, NULL on failure.
 */
ext2_dir_entry_t *ext2_find_dir_entry(inode_t *dir, const char *name);

/**
 * ext2_get_dir_entry_type - Convert Ext2 file type to VFS type
 * @ext2_type: Ext2 file type
 *
 * Converts EXT2_FT_* to FS_FILE_TYPE_*.
 * Returns VFS file type.
 */
uint32_t ext2_get_dir_entry_type(uint8_t ext2_type);

/**
 * ext2_set_dir_entry_type - Convert VFS type to Ext2 file type
 * @vfs_type: VFS file type
 *
 * Converts FS_FILE_TYPE_* to EXT2_FT_*.
 * Returns Ext2 file type.
 */
uint8_t ext2_set_dir_entry_type(uint32_t vfs_type);

#endif /* FS_EXT2_DIRECTORY_H */