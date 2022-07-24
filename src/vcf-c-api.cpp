/*
  C API provides an application binary interface for external use
 */

extern "C" {
#include "vcf-c-api.h"
}

#include "Variant.h"

using namespace std;
using namespace vcflib;

void testme() {
}

void *zig_variant(void *var) {
    return 0L;
}

const char *get_id(void *var) {
    auto v = static_cast<Variant*>(var);

    cout << "BACK IN C++ getname " << v->id << endl;
    return (v->id.data());
}
