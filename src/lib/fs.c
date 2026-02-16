/*
 * lib/fs.c – High-level filesystem abstraction library.
 *
 * Wraps the VFS and ext2 layers to provide simple, one-call
 * operations.  All path splitting, inode lookup, and reference-
 * counting is handled internally so callers only deal with paths
 * and buffers.
 */

#include <lib/fs.h>
#include <fs/vfs/file.h>
#include <fs/vfs/inode.h>
#include <fs/vfs/dentry.h>
#include <fs/ext2/ext2.h>
#include <fs/ext2/directory.h>
#include <kernel/heap-allocator.h>
#include <lib/string.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/*
 * Split an absolute path into its parent directory and the final
 * name component.  E.g. "/a/b/c" -> parent="/a/b", name="c".
 * Returns 0 on success, -1 if the path is root or malformed.
 */
static int split_absolute_path(const char *path,
                      char *parent, int pmax,
                      char *name,   int nmax)
{
    int len = strlen(path);
    if (len <= 1) return -1;
    if (path[0] != '/') return -1;

    int end = len - 1;
    while (end > 0 && path[end] == '/') end--;

    int slash = end;
    while (slash > 0 && path[slash] != '/') slash--;

    int nstart = slash + 1;
    int nlen   = end - nstart + 1;
    if (nlen <= 0 || nlen >= nmax) return -1;
    memcpy(name, path + nstart, nlen);
    name[nlen] = '\0';

    if (slash == 0) {
        parent[0] = '/';
        parent[1] = '\0';
    } else {
        if (slash >= pmax) return -1;
        memcpy(parent, path, slash);
        parent[slash] = '\0';
    }
    return 0;
}

/*
 * Resolve the parent directory of `path` and extract the final name
 * component.  Returns the parent inode (caller must inode_put),
 * or NULL on failure.  On NULL, *err is set to a specific fs_err code.
 */
static inode_t *resolve_parent(const char *path, char *name_out, int nmax,
                               short *err)
{
    char parent_path[256];
    if (split_absolute_path(path, parent_path, 256, name_out, nmax) != 0) {
        *err = FS_ERR_INVAL;
        return NULL;
    }
    inode_t *parent = path_lookup(parent_path);
    if (!parent) {
        *err = FS_ERR_NOT_FOUND;
        return NULL;
    }
    return parent;
}

/* ------------------------------------------------------------------ */
/*  File operations                                                    */
/* ------------------------------------------------------------------ */

/* Resolve parent, call create with 0644 permissions, release inode. */
short fs_create_file(const char *path)
{
    if (!path) return FS_ERR_INVAL;

    short err = FS_ERR;
    char name[256];
    inode_t *parent = resolve_parent(path, name, 256, &err);
    if (!parent) return err;

    int rc = parent->f_ops->create(parent, name, 0644);
    inode_put(parent);
    return rc == 0 ? FS_OK : FS_ERR_IO;
}

/* Resolve parent, call unlink, release inode. */
short fs_remove_file(const char *path)
{
    if (!path) return FS_ERR_INVAL;

    short err = FS_ERR;
    char name[256];
    inode_t *parent = resolve_parent(path, name, 256, &err);
    if (!parent) return err;

    int rc = parent->f_ops->unlink(parent, name);
    inode_put(parent);
    return rc == 0 ? FS_OK : FS_ERR_IO;
}

/* Open for read, loop file_read until EOF or buffer full, close. */
size_t fs_read_file(const char *path, void *buf, size_t buf_size)
{
    if (!path || !buf || buf_size == 0) return (size_t)FS_ERR_INVAL;

    file_t *f = file_open(path, FILE_FLAG_READ);
    if (!f) return (size_t)FS_ERR_NOT_FOUND;

    size_t total = 0;
    char *dst = (char *)buf;
    while (total < buf_size) {
        int n = file_read(f, dst + total, buf_size - total);
        if (n <= 0) break;
        total += (size_t)n;
    }

    file_close(f);
    return total;
}

/* Open for write (offset 0), write once, close. */
size_t fs_write_file(const char *path, const void *data, size_t len)
{
    if (!path || !data) return (size_t)FS_ERR_INVAL;

    file_t *f = file_open(path, FILE_FLAG_WRITE);
    if (!f) return (size_t)FS_ERR_NOT_FOUND;

    int n = file_write(f, data, len);
    file_close(f);
    return n < 0 ? (size_t)FS_ERR_IO : (size_t)n;
}

/* Open for write+append, write once, close. */
size_t fs_append_file(const char *path, const void *data, size_t len)
{
    if (!path || !data) return (size_t)FS_ERR_INVAL;

    file_t *f = file_open(path, FILE_FLAG_WRITE | FILE_FLAG_APPEND);
    if (!f) return (size_t)FS_ERR_NOT_FOUND;

    int n = file_write(f, data, len);
    file_close(f);
    return n < 0 ? (size_t)FS_ERR_IO : (size_t)n;
}

/* ------------------------------------------------------------------ */
/*  Directory operations                                               */
/* ------------------------------------------------------------------ */

/* Resolve parent, call mkdir with 0755 permissions, release inode. */
short fs_create_dir(const char *path)
{
    if (!path) return FS_ERR_INVAL;

    short err = FS_ERR;
    char name[256];
    inode_t *parent = resolve_parent(path, name, 256, &err);
    if (!parent) return err;

    int rc = parent->f_ops->mkdir(parent, name, 0755);
    inode_put(parent);
    return rc == 0 ? FS_OK : FS_ERR_IO;
}

/* Resolve parent, call rmdir, release inode. */
short fs_remove_dir(const char *path)
{
    if (!path) return FS_ERR_INVAL;

    short err = FS_ERR;
    char name[256];
    inode_t *parent = resolve_parent(path, name, 256, &err);
    if (!parent) return err;

    int rc = parent->f_ops->rmdir(parent, name);
    inode_put(parent);
    return rc == 0 ? FS_OK : FS_ERR_IO;
}

/*
 * Resolve directory inode, read raw directory data via ext2_readdir,
 * parse ext2_dir_entry_t records, optionally skip "." and "..",
 * fill caller's array, release all resources.
 */
size_t fs_list_dir(const char *path, fs_dir_entry_t *entries,
                   size_t max_entries, bool skip_dot)
{
    if (!path || !entries || max_entries == 0) return (size_t)FS_ERR_INVAL;

    inode_t *dir = path_lookup(path);
    if (!dir) return (size_t)FS_ERR_NOT_FOUND;
    if (!FS_IS_DIR(dir->mode)) {
        inode_put(dir);
        return (size_t)FS_ERR_NOT_DIR;
    }

    uint32_t dir_size = dir->size;
    if (dir_size == 0) {
        inode_put(dir);
        return 0;
    }

    uint8_t *raw = (uint8_t *)kmalloc(dir_size);
    if (!raw) {
        inode_put(dir);
        return (size_t)FS_ERR_NOMEM;
    }

    int bytes = ext2_readdir(dir, raw, dir_size, 0);
    inode_put(dir);

    if (bytes <= 0) {
        kfree((uintptr_t)raw);
        return (bytes == 0) ? 0 : (size_t)FS_ERR_IO;
    }

    /* Walk the variable-length ext2 directory entries. */
    size_t count = 0;
    uint32_t off = 0;

    while (off < (uint32_t)bytes && count < max_entries) {
        ext2_dir_entry_t *de = (ext2_dir_entry_t *)(raw + off);
        if (de->rec_len == 0) break;

        if (de->inode != 0) {
            int is_dot = (de->name_len == 1 && de->name[0] == '.') ||
                         (de->name_len == 2 && de->name[0] == '.'
                                            && de->name[1] == '.');
            if (!skip_dot || !is_dot) {
                uint8_t nlen = de->name_len;
                memcpy(entries[count].name, de->name, nlen);
                entries[count].name[nlen] = '\0';

                entries[count].ino = de->inode;

                switch (de->file_type) {
                case EXT2_FT_REG_FILE: entries[count].type = FS_ENTRY_FILE; break;
                case EXT2_FT_DIR:      entries[count].type = FS_ENTRY_DIR;  break;
                case EXT2_FT_SYMLINK:  entries[count].type = FS_ENTRY_LINK; break;
                default:               entries[count].type = FS_ENTRY_UNKNOWN; break;
                }
                count++;
            }
        }
        off += de->rec_len;
    }

    kfree((uintptr_t)raw);
    return count;
}

/* ------------------------------------------------------------------ */
/*  Query / metadata operations                                        */
/* ------------------------------------------------------------------ */

/* Open file, call file_stat, close. */
short fs_stat(const char *path, fs_stat_t *st)
{
    if (!path || !st) return FS_ERR_INVAL;

    file_t *f = file_open(path, FILE_FLAG_READ);
    if (!f) return FS_ERR_NOT_FOUND;

    int rc = file_stat(f, st);
    file_close(f);
    return rc == 0 ? FS_OK : FS_ERR_IO;
}

/* Resolve inode, release immediately; success = exists. */
short fs_exists(const char *path)
{
    if (!path) return 0;

    inode_t *inode = path_lookup(path);
    if (!inode) return 0;
    inode_put(inode);
    return 1;
}

/* Resolve inode, check FS_IS_DIR, release. */
short fs_is_dir(const char *path)
{
    if (!path) return 0;

    inode_t *inode = path_lookup(path);
    if (!inode) return 0;

    short result = FS_IS_DIR(inode->mode) ? 1 : 0;
    inode_put(inode);
    return result;
}

/* Resolve inode, check FS_IS_FILE, release. */
short fs_is_file(const char *path)
{
    if (!path) return 0;

    inode_t *inode = path_lookup(path);
    if (!inode) return 0;

    short result = FS_IS_FILE(inode->mode) ? 1 : 0;
    inode_put(inode);
    return result;
}

/* Resolve inode, return size if regular file, release. */
size_t fs_file_size(const char *path)
{
    if (!path) return (size_t)FS_ERR_INVAL;

    inode_t *inode = path_lookup(path);
    if (!inode) return (size_t)FS_ERR_NOT_FOUND;

    if (!FS_IS_FILE(inode->mode)) {
        inode_put(inode);
        return (size_t)FS_ERR_NOT_FILE;
    }

    size_t size = (size_t)inode->size;
    inode_put(inode);
    return size;
}
