#include <lib/stdio.h>
#include <stdbool.h>


void shell_loop1() {
    printf("tests");
    while (true) {

        printf("-");
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
}

void shell_loop2() {
    printf("tests");
    while (true) {

        printf("#");
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
}

void shell_loop3() {
    printf("tests");
    while (true) {

        printf("+");
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
}