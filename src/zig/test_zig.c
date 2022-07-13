
#include <stdio.h>

extern char *hello_zig(char *msg);

int main(int argc, char **argv) {
    printf("%s\n",hello_zig("hello world from C!"));
    return 0;
}
