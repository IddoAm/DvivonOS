/*
 * prog/shell.c – Minimal interactive filesystem test shell.
 *
 * Provides ls / cat / touch / write / mkdir / rm / rmdir / stat / help / exit.
 * Uses keyboard_read() for input and printf() for output.
 * Also includes test_filesystem(), an automated smoke-test sequence.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/stdio.h>
#include <lib/string.h>
#include <drivers/keyboard.h>

#include <fs/vfs/mount.h>
#include <fs/vfs/superblock.h>
#include <fs/vfs/inode.h>
#include <fs/vfs/file.h>
#include <fs/vfs/dentry.h>
#include <fs/ext2/ext2.h>
#include <fs/ext2/block.h>

#include <kernel/heap-allocator.h>

/* ------------------------------------------------------------------ */
/*  Readline – blocking line input from keyboard                      */
/* ------------------------------------------------------------------ */

/* Read a line into buf (up to max-1 chars). Echoes to terminal.       */
static void shell_readline(char *buf, int max)
{
    int pos = 0;
    while (1) {
        key_event ev;
        while (!keyboard_read(&ev))
            __asm__ volatile("hlt");

        if (!ev.pressed) continue;

        if (ev.code == KC_ENTER) {
            buf[pos] = '\0';
            putc('\n');
            return;
        }
        if (ev.code == KC_BSPC) {
            if (pos > 0) {
                pos--;
                putc('\b');
            }
            continue;
        }
        if (ev.ascii >= ' ' && ev.ascii <= '~' && pos < max - 1) {
            buf[pos++] = ev.ascii;
            putc(ev.ascii);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Skip leading whitespace and return pointer to first non-space char. */
static const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* Extract next whitespace-delimited token from *cursor into dst.
   Advances *cursor past the token + trailing spaces.  Returns 1 if
   a token was found, 0 if end-of-string.                              */
static int next_token(const char **cursor, char *dst, int max)
{
    const char *p = skip_spaces(*cursor);
    if (*p == '\0') { dst[0] = '\0'; return 0; }

    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && i < max - 1)
        dst[i++] = *p++;
    dst[i] = '\0';
    *cursor = p;
    return 1;
}

/* Resolve a path to an inode.  Handles "/" specially.                  */
static inode_t *resolve(const char *path)
{
    return path_lookup(path);
}

/* Given "/foo/bar/baz", split into parent="/foo/bar" and name="baz".
   Parent path goes to pbuf (size pmax), name to nbuf (size nmax).
   Returns 0 on success, -1 if the path is root or malformed.          */
static int split_parent_name(const char *path, char *pbuf, int pmax,
                             char *nbuf, int nmax)
{
    int len = strlen(path);
    if (len <= 1) return -1;        /* "/" has no parent */

    /* find last '/' */
    int last = len - 1;
    while (last > 0 && path[last] == '/') last--;   /* trim trailing / */
    int slash = last;
    while (slash > 0 && path[slash] != '/') slash--;

    /* name part */
    int nstart = slash + 1;
    int nlen = last - nstart + 1;
    if (nlen <= 0 || nlen >= nmax) return -1;
    memcpy(nbuf, path + nstart, nlen);
    nbuf[nlen] = '\0';

    /* parent part */
    if (slash == 0) {
        pbuf[0] = '/'; pbuf[1] = '\0';
    } else {
        if (slash >= pmax) return -1;
        memcpy(pbuf, path, slash);
        pbuf[slash] = '\0';
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Command: ls                                                        */
/* ------------------------------------------------------------------ */

static void cmd_ls(const char *path)
{
    if (!path || path[0] == '\0') path = "/";

    inode_t *dir = resolve(path);
    if (!dir) { printf("ls: cannot access '%s'\n", path); return; }
    if (!FS_IS_DIR(dir->mode)) {
        printf("ls: '%s' is not a directory\n", path);
        inode_put(dir);
        return;
    }

    ext2_fs_data_t    *fs = (ext2_fs_data_t *)dir->sb->fs_data;
    ext2_inode_data_t *ei = (ext2_inode_data_t *)dir->fs_data;
    uint32_t bs = fs->block_size;
    uint32_t dir_blocks = (dir->size + bs - 1) / bs;

    uint8_t *bbuf = (uint8_t *)kmalloc(bs);
    if (!bbuf) { inode_put(dir); return; }

    for (uint32_t b = 0; b < dir_blocks; b++) {
        uint32_t phys = ext2_resolve_block_num(
            fs->dev, &ei->disk_inode, b, bs);
        if (phys == 0) continue;
        ext2_read_block(fs->dev, phys, bbuf);

        uint32_t off = 0;
        while (off < bs) {
            ext2_dir_entry_t *de = (ext2_dir_entry_t *)(bbuf + off);
            if (de->rec_len == 0) break;

            if (de->inode != 0) {
                char name[256];
                memcpy(name, de->name, de->name_len);
                name[de->name_len] = '\0';

                const char *tag;
                switch (de->file_type) {
                case EXT2_FT_DIR:      tag = "DIR "; break;
                case EXT2_FT_REG_FILE: tag = "FILE"; break;
                case EXT2_FT_SYMLINK:  tag = "LINK"; break;
                default:               tag = " -- "; break;
                }
                printf("  [%s] ino=%d  %s\n", tag, de->inode, name);
            }
            off += de->rec_len;
        }
    }

    kfree((uintptr_t)bbuf);
    inode_put(dir);
}

/* ------------------------------------------------------------------ */
/*  Command: cat                                                       */
/* ------------------------------------------------------------------ */

static void cmd_cat(const char *path)
{
    if (!path || path[0] == '\0') {
        printf("cat: missing path\n");
        return;
    }

    file_t *f = file_open(path, FILE_FLAG_READ);
    if (!f) { printf("cat: cannot open '%s'\n", path); return; }

    char tmp[129];
    int n;
    while ((n = file_read(f, tmp, 128)) > 0) {
        tmp[n] = '\0';
        printf("%s", tmp);
    }
    printf("\n");
    file_close(f);
}

/* ------------------------------------------------------------------ */
/*  Command: touch (create empty file)                                 */
/* ------------------------------------------------------------------ */

static void cmd_touch(const char *path)
{
    if (!path || path[0] == '\0') {
        printf("touch: missing path\n");
        return;
    }

    char pbuf[256], nbuf[256];
    if (split_parent_name(path, pbuf, 256, nbuf, 256) != 0) {
        printf("touch: invalid path\n");
        return;
    }

    inode_t *parent = resolve(pbuf);
    if (!parent) { printf("touch: parent '%s' not found\n", pbuf); return; }

    int rc = parent->f_ops->create(parent, nbuf, 0644);
    inode_put(parent);
    printf("touch: %s -> %s\n", path, rc == 0 ? "OK" : "FAIL");
}

/* ------------------------------------------------------------------ */
/*  Command: write <path> <text>                                       */
/* ------------------------------------------------------------------ */

static void cmd_write(const char *args)
{
    char path[256];
    const char *cursor = args;
    if (!next_token(&cursor, path, 256)) {
        printf("write: usage: write <path> <text>\n");
        return;
    }
    const char *text = skip_spaces(cursor);
    if (*text == '\0') {
        printf("write: missing text\n");
        return;
    }
    file_t *f = file_open(path, FILE_FLAG_WRITE);
    if (!f) { printf("write: cannot open '%s'\n", path); return; }
    int n = file_write(f, text, strlen(text));
    file_close(f);
    printf("write: wrote %d bytes to %s\n", n, path);
}

/* ------------------------------------------------------------------ */
/*  Command: mkdir                                                     */
/* ------------------------------------------------------------------ */

static void cmd_mkdir(const char *path)
{
    if (!path || path[0] == '\0') {
        printf("mkdir: missing path\n");
        return;
    }

    char pbuf[256], nbuf[256];
    if (split_parent_name(path, pbuf, 256, nbuf, 256) != 0) {
        printf("mkdir: invalid path\n");
        return;
    }

    inode_t *parent = resolve(pbuf);
    if (!parent) { printf("mkdir: parent '%s' not found\n", pbuf); return; }

    int rc = parent->f_ops->mkdir(parent, nbuf, 0755);
    inode_put(parent);
    printf("mkdir: %s -> %s\n", path, rc == 0 ? "OK" : "FAIL");
}

/* ------------------------------------------------------------------ */
/*  Command: rm (unlink)                                               */
/* ------------------------------------------------------------------ */

static void cmd_rm(const char *path)
{
    if (!path || path[0] == '\0') {
        printf("rm: missing path\n");
        return;
    }

    char pbuf[256], nbuf[256];
    if (split_parent_name(path, pbuf, 256, nbuf, 256) != 0) {
        printf("rm: invalid path\n");
        return;
    }
    inode_t *parent = resolve(pbuf);
    if (!parent) { printf("rm: parent '%s' not found\n", pbuf); return; }
    int rc = parent->f_ops->unlink(parent, nbuf);
    inode_put(parent);
    printf("rm: %s -> %s\n", path, rc == 0 ? "OK" : "FAIL");
}

/* ------------------------------------------------------------------ */
/*  Command: rmdir                                                     */
/* ------------------------------------------------------------------ */

static void cmd_rmdir(const char *path)
{
    if (!path || path[0] == '\0') {
        printf("rmdir: missing path\n");
        return;
    }

    char pbuf[256], nbuf[256];
    if (split_parent_name(path, pbuf, 256, nbuf, 256) != 0) {
        printf("rmdir: invalid path\n");
        return;
    }
    if (strcmp(nbuf, ".") == 0 || strcmp(nbuf, "..") == 0) {
        printf("rmdir: cannot remove '.' or '..'\n");
        return;
    }

    inode_t *parent = resolve(pbuf);
    if (!parent) { printf("rmdir: parent '%s' not found\n", pbuf); return; }

    int rc = parent->f_ops->rmdir(parent, nbuf);
    inode_put(parent);
    printf("rmdir: %s -> %s\n", path, rc == 0 ? "OK" : "FAIL");
}

/* ------------------------------------------------------------------ */
/*  Command: stat                                                      */
/* ------------------------------------------------------------------ */

static void cmd_stat(const char *path)
{
    if (!path || path[0] == '\0') {
        printf("stat: missing path\n");
        return;
    }

    inode_t *inode = resolve(path);
    if (!inode) { printf("stat: '%s' not found\n", path); return; }

    const char *type = "???";
    if (FS_IS_DIR(inode->mode))  type = "DIR";
    if (FS_IS_FILE(inode->mode)) type = "FILE";
    if (FS_IS_LINK(inode->mode)) type = "LINK";

    printf("  path  : %s\n", path);
    printf("  ino   : %d\n", inode->ino);
    printf("  type  : %s\n", type);
    printf("  mode  : 0%o\n", inode->mode & 0xFFF);
    printf("  size  : %d bytes\n", inode->size);
    printf("  nlink : %d\n", inode->nlink);
    printf("  blocks: %d\n", inode->blocks);

    inode_put(inode);
}

/* ------------------------------------------------------------------ */
/*  Command dispatch                                                   */
/* ------------------------------------------------------------------ */

static void shell_dispatch(const char *line)
{
    char cmd[64];
    const char *cursor = line;

    if (!next_token(&cursor, cmd, 64)) return;   /* empty line */

    if (strcmp(cmd, "ls") == 0) {
        char arg[256];
        if (!next_token(&cursor, arg, 256))
            cmd_ls("/");
        else
            cmd_ls(arg);

    } else if (strcmp(cmd, "cat") == 0) {
        char arg[256];
        next_token(&cursor, arg, 256);
        cmd_cat(arg);

    } else if (strcmp(cmd, "touch") == 0) {
        char arg[256];
        next_token(&cursor, arg, 256);
        cmd_touch(arg);

    } else if (strcmp(cmd, "write") == 0) {
        /* pass the rest of the line (path + text) */
        cmd_write(cursor);

    } else if (strcmp(cmd, "mkdir") == 0) {
        char arg[256];
        next_token(&cursor, arg, 256);
        cmd_mkdir(arg);

    } else if (strcmp(cmd, "rm") == 0) {
        char arg[256];
        next_token(&cursor, arg, 256);
        cmd_rm(arg);

    } else if (strcmp(cmd, "rmdir") == 0) {
        char arg[256];
        next_token(&cursor, arg, 256);
        cmd_rmdir(arg);

    } else if (strcmp(cmd, "stat") == 0) {
        char arg[256];
        next_token(&cursor, arg, 256);
        cmd_stat(arg);

    } else if (strcmp(cmd, "help") == 0) {
        printf("  ls [path]           list directory\n");
        printf("  cat <path>          print file contents\n");
        printf("  touch <path>        create empty file\n");
        printf("  write <path> <txt>  write text to file\n");
        printf("  mkdir <path>        create directory\n");
        printf("  rm <path>           remove file\n");
        printf("  rmdir <path>        remove empty directory\n");
        printf("  stat <path>         show inode metadata\n");
        printf("  exit                leave the shell\n");

    } else {
        printf("unknown command: '%s' (type 'help')\n", cmd);
    }
}

/* ------------------------------------------------------------------ */
/*  Interactive shell entry-point                                      */
/* ------------------------------------------------------------------ */

void fs_test_shell(void)
{
    mount_t *mnt = get_root_mount();
    if (!mnt) {
        printf("[fs-shell] No filesystem mounted. Shell unavailable.\n");
        return;
    }

    printf("\n=== FS Test Shell (type 'help' for commands) ===\n");

    char line[256];
    while (1) {
        printf("fs> ");
        shell_readline(line, sizeof(line));

        if (strcmp(line, "exit") == 0)
            break;

        shell_dispatch(line);
    }

    printf("[fs-shell] exited.\n");
}

/* ------------------------------------------------------------------ */
/*  Automated test suite                                               */
/* ------------------------------------------------------------------ */

/* Small pass/fail counter */
static int t_pass, t_fail;

static void T(const char *name, int ok)
{
    if (ok) { printf("  [PASS] %s\n", name); t_pass++; }
    else    { printf("  [FAIL] %s\n", name); t_fail++; }
}

void test_filesystem(void)
{
    t_pass = t_fail = 0;

    printf("\n=== Filesystem Automated Tests ===\n\n");

    /* 0. Check mount */
    mount_t *mnt = get_root_mount();
    T("root mount exists", mnt != NULL);
    if (!mnt) {
        printf("  Cannot continue without a mounted filesystem.\n");
        return;
    }

    superblock_t *sb = mnt->sb;
    inode_t *root = sb->root;
    T("root inode is a directory", root && FS_IS_DIR(root->mode));

    /* 1. ls / (should contain at least . or lost+found) */
    printf("\n-- ls / --\n");
    cmd_ls("/");

    /* 2. Create a file */
    printf("\n-- create /test.txt --\n");
    int rc = root->f_ops->create(root, "test.txt", 0644);
    T("create /test.txt", rc == 0);

    /* 3. Write to the file */
    printf("\n-- write /test.txt --\n");
    file_t *f = file_open("/test.txt", FILE_FLAG_WRITE);
    T("open /test.txt for write", f != NULL);
    if (f) {
        const char *msg = "Hello from ext2 test!";
        int written = file_write(f, msg, strlen(msg));
        T("write 21 bytes", written == 21);
        file_close(f);
    }

    /* 4. Read it back */
    printf("\n-- cat /test.txt --\n");
    f = file_open("/test.txt", FILE_FLAG_READ);
    T("open /test.txt for read", f != NULL);
    if (f) {
        char buf[64];
        memset(buf, 0, sizeof(buf));
        int n = file_read(f, buf, sizeof(buf) - 1);
        T("read back > 0 bytes", n > 0);
        if (n > 0) {
            buf[n] = '\0';
            printf("  content: \"%s\"\n", buf);
            T("content matches", strcmp(buf, "Hello from ext2 test!") == 0);
        }
        file_close(f);
    }

    /* 5. stat */
    printf("\n-- stat /test.txt --\n");
    cmd_stat("/test.txt");

    /* 6. mkdir */
    printf("\n-- mkdir /mydir --\n");
    rc = root->f_ops->mkdir(root, "mydir", 0755);
    T("mkdir /mydir", rc == 0);

    /* 7. ls / after create + mkdir */
    printf("\n-- ls / (after changes) --\n");
    cmd_ls("/");

    /* 8. Create file inside subdir */
    printf("\n-- touch /mydir/inner.txt --\n");
    inode_t *mydir = path_lookup("/mydir");
    T("lookup /mydir", mydir != NULL);
    if (mydir) {
        rc = mydir->f_ops->create(mydir, "inner.txt", 0644);
        T("create inner.txt", rc == 0);
        inode_put(mydir);
    }

    printf("\n-- ls /mydir --\n");
    cmd_ls("/mydir");

    /* 9. Cleanup – rm file, rmdir */
    printf("\n-- rm /test.txt --\n");
    rc = root->f_ops->unlink(root, "test.txt");
    T("unlink /test.txt", rc == 0);

    /* Remove inner.txt first, then mydir */
    printf("\n-- rm /mydir/inner.txt --\n");
    mydir = path_lookup("/mydir");
    if (mydir) {
        rc = mydir->f_ops->unlink(mydir, "inner.txt");
        T("unlink inner.txt", rc == 0);
        inode_put(mydir);
    }

    printf("\n-- rmdir /mydir --\n");
    rc = root->f_ops->rmdir(root, "mydir");
    T("rmdir /mydir", rc == 0);

    /* 10. Final ls */
    printf("\n-- ls / (after cleanup) --\n");
    cmd_ls("/");

    printf("\n=== Results: %d passed, %d failed ===\n", t_pass, t_fail);
}
