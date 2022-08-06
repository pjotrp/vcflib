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

void *var_parse(const char *line, bool parse_samples) {
    Variant * var = new Variant(); // construct buffer
    // Variant::parse(string& line, bool parseSamples) {
    string s = line;
    var->parse(s, parse_samples);
    cerr << "HEY\n" << s << "{" << var->id << "}" << endl;
    printf("<%p %s>\n",var,var->id.c_str());
    return var;
}

const char *var_id(void *var) {
    auto v = static_cast<Variant*>(var);
    // cout << "BACK IN C++ getname " << v->id << endl;
    return (v->id.data());
}

const long var_pos(void *var) {
    return (static_cast<Variant*>(var)->position);
}

const char *var_ref(void *var) {
    auto v = static_cast<Variant*>(var);
    return (v->ref.data());
}

void var_set_id(void *var, const char *id) {
    auto v = static_cast<Variant*>(var);
    v->id = id;
}

void var_set_ref(void *var, const char *ref) {
    auto v = static_cast<Variant*>(var);
    v->ref = ref;
}
