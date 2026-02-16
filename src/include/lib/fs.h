#ifndef LIB_FS_H
#define LIB_FS_H

/*
 * lib/fs.h – High-level filesystem abstraction library.
 *
 * Provides simple, one-call operations for common filesystem tasks
 * (read, write, create, remove, list, stat) so callers never need
 * to deal with inodes, path splitting, or reference counting.
 *
 * All functions return short.  Negative values are error codes from
 * the fs_err enum; zero or positive values indicate success.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <fs/types.h>

/* ------------------------------------------------------------------ */
/*  Error codes  (negative = error, 0 = success, >0 = data/count)     */
/* ------------------------------------------------------------------ */

enum fs_err {
    FS_OK            =   0,   /* success                               */
    FS_ERR           =  -1,   /* generic / unspecified error            */
    FS_ERR_INVAL     =  -2,   /* invalid argument (NULL, bad path)     */
    FS_ERR_NOT_FOUND =  -3,   /* path does not exist                   */
    FS_ERR_NOT_DIR   =  -4,   /* expected a directory                  */
    FS_ERR_NOT_FILE  =  -5,   /* expected a regular file               */
    FS_ERR_IO        =  -6,   /* I/O or underlying VFS error           */
    FS_ERR_NOMEM     =  -7,   /* out of memory (kmalloc failed)        */
};

/* ------------------------------------------------------------------ */
/*  Directory entry types                                              */
/* ------------------------------------------------------------------ */

#define FS_ENTRY_UNKNOWN  0
#define FS_ENTRY_FILE     1
#define FS_ENTRY_DIR      2
#define FS_ENTRY_LINK     3

/**
 * fs_dir_entry_t – One entry returned by fs_list_dir().
 *
 * name  : Null-terminated file/directory name.
 * ino   : Inode number on disk.
 * type  : FS_ENTRY_FILE, FS_ENTRY_DIR, FS_ENTRY_LINK, or FS_ENTRY_UNKNOWN.
 */
typedef struct {
    char     name[256];
    uint32_t ino;
    uint8_t  type;
} fs_dir_entry_t;

/* ================================================================== */
/*  File operations                                                    */
/* ================================================================== */

/**
 * fs_create_file – Create a new empty file (permissions 0644).
 *
 * Input:  path – absolute path, e.g. "/docs/notes.txt".
 * Output: FS_OK on success, negative fs_err on failure.
 */
short fs_create_file(const char *path);

/**
 * fs_remove_file – Delete a regular file.
 *
 * Input:  path – absolute path of the file to delete.
 * Output: FS_OK on success, negative fs_err on failure.
 */
short fs_remove_file(const char *path);

/**
 * fs_read_file – Read an entire file into a buffer.
 *
 * Input:  path     – absolute path of the file.
 *         buf      – destination buffer.
 *         buf_size – maximum number of bytes to read.
 * Output: Number of bytes actually read (>= 0), or negative fs_err.
 */
size_t fs_read_file(const char *path, void *buf, size_t buf_size);

/**
 * fs_write_file – Write data to a file from the beginning (offset 0).
 *
 * Input:  path – absolute path of an existing file.
 *         data – source bytes.
 *         len  – number of bytes to write.
 * Output: Number of bytes written (>= 0), or negative fs_err.
 *
 * Note:   Does NOT truncate – bytes beyond `len` keep their old content.
 *         To fully overwrite, remove + create + write.
 */
size_t fs_write_file(const char *path, const void *data, size_t len);

/**
 * fs_append_file – Append data to the end of a file.
 *
 * Input:  path – absolute path of an existing file.
 *         data – source bytes.
 *         len  – number of bytes to append.
 * Output: Number of bytes written (>= 0), or negative fs_err.
 */
size_t fs_append_file(const char *path, const void *data, size_t len);

/* ================================================================== */
/*  Directory operations                                               */
/* ================================================================== */

/**
 * fs_create_dir – Create a new directory (permissions 0755).
 *
 * Input:  path – absolute path, e.g. "/projects/new_dir".
 * Output: FS_OK on success, negative fs_err on failure.
 */
short fs_create_dir(const char *path);

/**
 * fs_remove_dir – Remove an empty directory.
 *
 * Input:  path – absolute path of the directory to remove.
 * Output: FS_OK on success, negative fs_err on failure.
 */
short fs_remove_dir(const char *path);

/**
 * fs_list_dir – List the contents of a directory.
 *
 * Input:  path        – absolute path (use "/" for root).
 *         entries     – caller-allocated array to fill.
 *         max_entries – capacity of the array.
 * Output: Number of entries stored (>= 0), or negative fs_err.
 *
 * The special entries "." and ".." are automatically skipped.
 */
size_t fs_list_dir(const char *path, fs_dir_entry_t *entries, size_t max_entries, bool skip_dot);

/* ================================================================== */
/*  Query / metadata operations                                        */
/* ================================================================== */

/**
 * fs_stat – Fill an fs_stat_t with a file's or directory's metadata.
 *
 * Input:  path – absolute path.
 *         st   – pointer to caller-owned fs_stat_t.
 * Output: FS_OK on success, negative fs_err if not found.
 */
short fs_stat(const char *path, fs_stat_t *st);

/**
 * fs_exists – Test whether a path exists.
 *
 * Input:  path – absolute path.
 * Output: 1 if the path exists, 0 if it does not.
 */
short fs_exists(const char *path);

/**
 * fs_is_dir – Test whether a path is a directory.
 *
 * Input:  path – absolute path.
 * Output: 1 if path exists AND is a directory, 0 otherwise.
 */
short fs_is_dir(const char *path);

/**
 * fs_is_file – Test whether a path is a regular file.
 *
 * Input:  path – absolute path.
 * Output: 1 if path exists AND is a regular file, 0 otherwise.
 */
short fs_is_file(const char *path);

/**
 * fs_file_size – Get a file's size in bytes.
 *
 * Input:  path – absolute path to a regular file.
 * Output: File size in bytes (>= 0), or negative fs_err.
 */
size_t fs_file_size(const char *path);

#endif /* LIB_FS_H */
