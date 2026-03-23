#include <stdio.h>
#include <stdbool.h>


int main() {
    char name[32];
    printf("enter your sigmosh\n");

    scanf("%31s", name);
    printf("hello, %s!\n", name);
    
    return 0;
}