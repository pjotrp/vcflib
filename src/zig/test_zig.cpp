#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
    extern const char *hello_zig(const char *msg);
    extern void zig_process_vector(long vsize, const void *v);
}

using namespace std;

int main(int argc, char **argv) {
    string s = "Hello from C++";
    printf("%s\n",hello_zig(s.data()));

    vector<string> genotypes{ "1/0", "1/.", "0/1" };

    vector<const char *> list;
    for (auto g: genotypes)
        list.push_back(g.data());
    printf("%s\n",list[0]);
    zig_process_vector(list.size(),list.data());

    return 0;
}
