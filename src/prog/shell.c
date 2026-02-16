/*
 * prog/shell.c – Minimal interactive filesystem test shell.
 *
 * Provides ls / cat / touch / write / mkdir / rm / rmdir / stat / help / exit.
 * Uses the lib/fs abstraction for all filesystem operations.
 * Also includes test_filesystem(), an automated smoke-test sequence.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/fs.h>
#include <drivers/keyboard.h>

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

/* Return a human-readable tag for an fs_dir_entry_t type. */
static const char *entry_type_tag(uint8_t type)
{
    switch (type) {
    case FS_ENTRY_DIR:  return "DIR ";
    case FS_ENTRY_FILE: return "FILE";
    case FS_ENTRY_LINK: return "LINK";
    default:            return " -- ";
    }
}

/* Return a human-readable string for a negative fs_err code. */
static const char *fs_err_str(short err)
{
    switch (err) {
    case FS_ERR:           return "error";
    case FS_ERR_INVAL:     return "invalid path";
    case FS_ERR_NOT_FOUND: return "not found";
    case FS_ERR_NOT_DIR:   return "not a directory";
    case FS_ERR_NOT_FILE:  return "not a file";
    case FS_ERR_IO:        return "I/O error";
    case FS_ERR_NOMEM:     return "out of memory";
    default:               return "unknown error";
    }
}

/* ------------------------------------------------------------------ */
/*  Command: ls                                                        */
/* ------------------------------------------------------------------ */

static void cmd_ls(const char *path)
{
    if (!path || path[0] == '\0') path = "/";

    fs_dir_entry_t entries[64];
    size_t n = fs_list_dir(path, entries, 64, true);

    if ((short)n < 0) {
        printf("ls: '%s': %s\n", path, fs_err_str((short)n));
        return;
    }

    for (size_t i = 0; i < n; i++)
        printf("  [%s] ino=%d  %s\n",
               entry_type_tag(entries[i].type),
               entries[i].ino,
               entries[i].name);
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

    char buf[4097];
    size_t n = fs_read_file(path, buf, sizeof(buf) - 1);
    if ((short)n < 0) {
        printf("cat: '%s': %s\n", path, fs_err_str((short)n));
        return;
    }

    buf[n] = '\0';
    printf("%s\n", buf);
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

    short rc = fs_create_file(path);
    if (rc < 0)
        printf("touch: '%s': %s\n", path, fs_err_str(rc));
    else
        printf("touch: %s -> OK\n", path);
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

    size_t n = fs_write_file(path, text, strlen(text));
    if ((short)n < 0)
        printf("write: '%s': %s\n", path, fs_err_str((short)n));
    else
        printf("write: wrote %d bytes to %s\n", (int)n, path);
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

    short rc = fs_create_dir(path);
    if (rc < 0)
        printf("mkdir: '%s': %s\n", path, fs_err_str(rc));
    else
        printf("mkdir: %s -> OK\n", path);
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

    short rc = fs_remove_file(path);
    if (rc < 0)
        printf("rm: '%s': %s\n", path, fs_err_str(rc));
    else
        printf("rm: %s -> OK\n", path);
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

    short rc = fs_remove_dir(path);
    if (rc < 0)
        printf("rmdir: '%s': %s\n", path, fs_err_str(rc));
    else
        printf("rmdir: %s -> OK\n", path);
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

    fs_stat_t st;
    short rc = fs_stat(path, &st);
    if (rc < 0) {
        printf("stat: '%s': %s\n", path, fs_err_str(rc));
        return;
    }

    const char *type = "???";
    if (FS_IS_DIR(st.mode))  type = "DIR";
    if (FS_IS_FILE(st.mode)) type = "FILE";
    if (FS_IS_LINK(st.mode)) type = "LINK";

    printf("  path  : %s\n", path);
    printf("  ino   : %d\n", st.ino);
    printf("  type  : %s\n", type);
    printf("  mode  : 0%o\n", st.mode & 0xFFF);
    printf("  size  : %d bytes\n", st.size);
    printf("  nlink : %d\n", st.nlink);
    printf("  blocks: %d\n", st.blocks);
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
    if (!fs_exists("/")) {
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
    T("root exists", fs_exists("/"));
    T("root is a directory", fs_is_dir("/"));
    if (!fs_exists("/")) {
        printf("  Cannot continue without a mounted filesystem.\n");
        return;
    }

    /* 1. ls / */
    printf("\n-- ls / --\n");
    cmd_ls("/");

    /* 2. Create a file */
    printf("\n-- create /test.txt --\n");
    short rc = fs_create_file("/test.txt");
    T("create /test.txt", rc == FS_OK);

    /* 3. Write to the file */
    printf("\n-- write /test.txt --\n");
    const char *msg = "Hello from ext2 test!";
    size_t written = fs_write_file("/test.txt", msg, strlen(msg));
    T("write 21 bytes", written == 21);

    /* 4. Read it back */
    printf("\n-- cat /test.txt --\n");
    char buf[64];
    memset(buf, 0, sizeof(buf));
    size_t n = fs_read_file("/test.txt", buf, sizeof(buf) - 1);
    T("read back > 0 bytes", (short)n > 0);
    if ((short)n > 0) {
        buf[n] = '\0';
        printf("  content: \"%s\"\n", buf);
        T("content matches", strcmp(buf, "Hello from ext2 test!") == 0);
    }

    /* 5. stat */
    printf("\n-- stat /test.txt --\n");
    cmd_stat("/test.txt");

    /* 6. mkdir */
    printf("\n-- mkdir /mydir --\n");
    rc = fs_create_dir("/mydir");
    T("mkdir /mydir", rc == FS_OK);

    /* 7. ls / after create + mkdir */
    printf("\n-- ls / (after changes) --\n");
    cmd_ls("/");

    /* 8. Create file inside subdir */
    printf("\n-- touch /mydir/inner.txt --\n");
    rc = fs_create_file("/mydir/inner.txt");
    T("create /mydir/inner.txt", rc == FS_OK);

    printf("\n-- ls /mydir --\n");
    cmd_ls("/mydir");

    /* 9. Cleanup – rm file, rmdir */
    printf("\n-- rm /test.txt --\n");
    rc = fs_remove_file("/test.txt");
    T("unlink /test.txt", rc == FS_OK);

    printf("\n-- rm /mydir/inner.txt --\n");
    rc = fs_remove_file("/mydir/inner.txt");
    T("unlink inner.txt", rc == FS_OK);

    printf("\n-- rmdir /mydir --\n");
    rc = fs_remove_dir("/mydir");
    T("rmdir /mydir", rc == FS_OK);

    /* 10. Final ls */
    printf("\n-- ls / (after cleanup) --\n");
    cmd_ls("/");

    printf("\n=== Results: %d passed, %d failed ===\n", t_pass, t_fail);
}
