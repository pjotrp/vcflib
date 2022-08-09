/*
  C API for vcflib
 */

// do not use extern "C" because this file ought to be ready for a C compiler (!C++)

// VCF constructors

void *var_parse(const char *line, bool parse_samples);

// VCF variant accessors
const char *var_id(void *variant);
const long var_pos(void *variant);
const char *var_ref(void *variant);
const unsigned long var_alt_num(void *variant);
const char **var_alt(void *variant, const char ** ret);

void var_set_id(void *variant, const char *);
void var_set_ref(void *variant, const char *);
void var_clear_alt(void *var);
void var_set_alt(void *var, const char *alt, long idx);

// Zig functionality

void *zig_create_multi_allelic(void *retvar, void *varlist[], long size);

// Some test functions
void testme();
void *zig_variant_window();
void zig_variant_window_cleanup(void *varwin);
void win_push(void *varwin, void *var);
long win_size(void *varwin);


char *hello_zig2(char *s);

void *zig_variant(void *var);
