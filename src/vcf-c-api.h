/*
  C API for vcflib
 */

// do not use extern "C" because this file ought to be ready for a C compiler (!C++)

// VCF variant accessors
const char *var_id(void *variant);
const long var_pos(void *variant);

void var_set_id(void *variant, const char *);

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
