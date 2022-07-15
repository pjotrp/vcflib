#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
    extern const char *hello_zig(const char *msg);
    extern void zig_process_vector(long vsize, const char **v);
    extern void zig_process_opaque_ptr(void *test);
    extern void call_c(void *test);
    extern const char *get_name(void *test);
}

using namespace std;

class Variant {
public:
    string name;
    long position;
    Variant(string n, long p) { name = n; position = p; };
    string get_name() { return name; };
    long get_position() { return position; } ;
};

void call_c(void *test) {
    auto v = static_cast<Variant*>(test);

    cout << "BACK IN C++ " << v->name << endl;
}

const char *get_name(void *variant) {
    auto v = static_cast<Variant*>(variant);

    cout << "BACK IN C++ getname " << v->name << endl;
    return (v->name.data());
}

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

    auto v = Variant("varname",4555);
    cout << v.get_name() << ":" << v.get_position() << endl;

    zig_process_opaque_ptr(&v);
    return 0;
}
