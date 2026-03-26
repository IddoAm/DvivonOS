#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "syscalls.h"

#define FG_LIST  "/conf/fg.list"
#define BIN_DIR  "/public-bin/"
#define MAX_FG   32
#define PROCESS_NAME_MAX 64
#define FG_BUFFER MAX_FG * PROCESS_NAME_MAX + MAX_FG
#define LINE_MAX 256
#define MAX_ARGS 32

static char fg_names[MAX_FG][64];
static int  fg_count = 0;
static char current_path[256] = "/";

/* ── Foreground list ─────────────────────────────────────────────────── */

static void load_fg_list(void) {
    int fd = open(FG_LIST, O_RDONLY);
    if (fd < 0) return;

    char buf[FG_BUFFER];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';

    char* p = buf;
    while (*p && fg_count < MAX_FG) {
        char* end = p;
        while (*end && *end != '\n') end++;
        int len = (int)(end - p);
        if (len > 0 && len < 64) {
            memcpy(fg_names[fg_count], p, len);
            fg_names[fg_count][len] = '\0';
            fg_count++;
        }
        else
        {
            printf("[WARN] Illegal FG name");
        }
        p = (*end == '\n') ? end + 1 : end;
    }
}

static int is_fg(const char* cmd) {
    for (int i = 0; i < fg_count; i++)
        if (strcmp(fg_names[i], cmd) == 0) return 1;
    return 0;
}

/* ── Path helpers ────────────────────────────────────────────────────── */

// Resolve path: if relative, prepend current_path.
static void resolve_path(const char* in, char* out, const int out_size) {
    if (in[0] == '/') {
        strncpy(out, in, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    // Relative: join current_path + "/" + in
    int cp_len = strlen(current_path);
    snprintf(out, out_size, "%s%s%s",
             current_path,
             (current_path[cp_len - 1] == '/') ? "" : "/",
             in);
}

/* ── Built-ins ───────────────────────────────────────────────────────── */

static void builtin_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        fputs(argv[i], stdout);
    }
    putchar('\n');
}

static void builtin_cd(const char* dir) {
    if (dir[0] == '\0') {
        return;
    }

    char resolved[256];
    resolve_path(dir, resolved, sizeof(resolved));

    // Verify the directory exists by trying to open it
    int fd = open(resolved, O_RDONLY);
    if (fd < 0) {
        printf("cd: no such directory: %s\n", resolved);
        return;
    }
    close(fd);
    strncpy(current_path, resolved, sizeof(current_path) - 1);
    current_path[sizeof(current_path) - 1] = '\0';
    // Ensure no trailing slash (except root)
    int len = strlen(current_path);
    if (len > 1 && current_path[len - 1] == '/')
        current_path[len - 1] = '\0';
}

static void builtin_mkdir(const char* dir) {
    if (dir[0] == "\0") return;
    char resolved[256];
    resolve_path(dir, resolved, sizeof(resolved));
    
    if (create_dir(resolved) != 0)
        printf("mkdir: failed: %s\n", resolved);

}

static void builtin_touch(const char* path) {
    if (!path) return;
    char resolved[256];
    resolve_path(path, resolved, sizeof(resolved));
    int fd = open(resolved, O_CREAT | O_WRONLY);
    if (fd < 0)
        printf("touch: failed: %s\n", resolved);
    else
        close(fd);
}

/* ── Tokenizer ───────────────────────────────────────────────────────── */

static int tokenize(char* line, char** argv, int max_args) {
    int argc = 0;
    char* p = line;
    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

/* ── Main loop ───────────────────────────────────────────────────────── */

int main(void) {
    printf("welcome to the terminal!\n");
    load_fg_list();

    char line[LINE_MAX];
    char* argv[MAX_ARGS];
    int argc;

    while (1) {
        printf("%s> ", current_path);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        // Strip trailing newline
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        argc = tokenize(line, argv, MAX_ARGS);
        if (argc == 0) continue;

        char* cmd = argv[0];

        /* ── Built-ins ── */
        if (strcmp(cmd, "echo") == 0) { builtin_echo(argc, argv); continue; }
        if (strcmp(cmd, "cd")   == 0) { builtin_cd(argc > 1 ? argv[1] : "/"); continue; }
        if (strcmp(cmd, "mkdir")== 0) { if (argc > 1) builtin_mkdir(argv[1]); continue; }
        if (strcmp(cmd, "touch")== 0) { if (argc > 1) builtin_touch(argv[1]); continue; }
        if (strcmp(cmd, "exit") == 0) break;


        /* ── External command ── */
        char bin_path[128];
        snprintf(bin_path, sizeof(bin_path), "%s%s", BIN_DIR, cmd);

        int fd = open(bin_path, O_RDONLY);
        if (fd < 0) {
            printf("command not found: %s\n", cmd);
            continue;
        }
        close(fd);

        proc_create_args_t args = {
            .path = bin_path,
            .argc = argc,
            .argv = (const char**)argv,
            .cwd  = current_path,
        };
        int pid = create_proc_by_elf(&args);
        if (pid < 0) {
            printf("failed to start: %s\n", cmd);
            continue;
        }

        if (is_fg(cmd))
        {
            wait_pid(pid);
            printf("\n");
        }
    }

    printf("terminal exiting..");

    return 0;
}
