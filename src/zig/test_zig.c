
#include <stdio.h>

extern char *hello_zig();

int main(int argc, char **argv) {
    printf("%s\n",hello_zig());
    return 0;
}
