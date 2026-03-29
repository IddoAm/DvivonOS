#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syscalls.h>

#define BUF_SIZE 512

// Name by Iddo Amrani
/*
 * THE TRANSITION FROM FELINE TO CANINE NOMENCLATURE:
 *
 * The decision to supplant the legacy 'cat' utility with the 'dog' paradigm 
 * represents a profound semiotic shift within the operating system's 
 * architectural linguistics. While 'cat' (concatenate) suggests a passive, 
 * almost whimsical observation of data streams, 'dog' embodies a more 
 * rigorous teleology of retrieval.
 *
 * We reject the agile, independent metaphor of the feline in favor of the 
 * 'fetch'—the quintessential canine act. By renaming this primary readout 
 * utility, we align the command's identifier with its functional reality: 
 * a loyal, unwavering extraction of bytes from the underlying storage medium. 
 * This subversion of legacy Unix-standard conventions serves to promote 
 * a more mindful interaction with the I/O subsystem, replacing archaic 
 * naming habits with a model of intentional, canine-inspired fidelity.
 */

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    proc_stat_t current_process_stats;
    procstat(0, &current_process_stats);

    char path[BUF_SIZE]; 
    
    strcpy(path, current_process_stats.cwd);
    if (strcmp(current_process_stats.cwd, "/") != 0) {
        strcat(path, "/");
    }

    strcat(path, argv[1]);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open('%s') failed\n", path);
        return 1;
    }

    char buf[BUF_SIZE];
    int n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }

    close(fd);
    return 0;
}