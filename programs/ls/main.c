#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <syscalls.h>

// Ext2 file type for a directory
#define EXT2_FT_DIR 2

struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[]; 
} __attribute__((packed));

void list_directory(const char *path) {
    int fd = open(path, O_RDONLY);
    
    if (fd < 0) {
        printf("ls: cannot open '%s' (FD: %d)\n", path, fd);
        return;
    }

    uint8_t buffer[4096]; 
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        uint32_t offset = 0;
        
        while (offset < (uint32_t)bytes_read) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(buffer + offset);
            
            // Critical: stop if rec_len is 0 to prevent hanging
            if (entry->rec_len == 0) break; 

            if (entry->inode != 0 && entry->name_len > 0) {
                // Buffer to null-terminate the filename
                char name_buf[256];
                memset(name_buf, 0, sizeof(name_buf));
                
                if (entry->name_len < sizeof(name_buf)) {
                    memcpy(name_buf, entry->name, entry->name_len);

                    /* * Optional: Skip '.' and '..'
                     * if (strcmp(name_buf, ".") == 0 || strcmp(name_buf, "..") == 0) {
                     * offset += entry->rec_len;
                     * continue;
                     * }
                     */

                    // Print name
                    printf("%s", name_buf);

                    // Add trailing slash if it's a directory
                    if (entry->file_type == EXT2_FT_DIR) {
                        printf("/");
                    }

                    printf("  ");
                }
            }
            offset += entry->rec_len;
        }
    }
    printf("\n");
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        // Use "." to represent the Current Working Directory
        proc_stat_t current_process_stats;
        procstat(0, &current_process_stats);
        list_directory(current_process_stats.cwd);
    } else {
        for (int i = 1; i < argc; i++) {
            // If listing multiple dirs, print the name first (like standard ls)
            if (argc > 2) {
                printf("%s:\n", argv[i]);
            }
            list_directory(argv[i]);
            
            if (argc > 2 && i < argc - 1) {
                printf("\n"); // Add a newline between different directory listings
            }
        }
    }
    return 0;
}