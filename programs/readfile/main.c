#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    const char* path = "/hello.txt";
    if (argc > 1)
        path = argv[1];

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open('%s') failed\n", path);
        return 1;
    }

    char buf[128];
    int n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }

    close(fd);
    return 0;
}
