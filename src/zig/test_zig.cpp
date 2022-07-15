#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
    extern const char *hello_zig(const char *msg);
    extern void zig_process_vector(long vsize, const char **v);
}

using namespace std;

int main(int argc, char **argv) {
    string s = "Hello from C++";
    printf("%s\n",hello_zig(s.data()));

    vector<string> genotypes{ "1/0", "2/.", "3/1" };
    vector<const char *> list;

    auto idx = 0;
    for (auto g: genotypes) {
        auto value = g;
        printf("push %s\n",value.data());
        list.push_back(genotypes[idx].data());
        idx++;
    }
    for (auto t: list) {
        printf("visit %s\n",t);
    }
    zig_process_vector(list.size(),list.data());

    return 0;
}
