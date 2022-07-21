/*
  C API for vcflib
 */

extern "C" {

    void *zig_variant_window();
    long win_size(void *varwin);
    char *hello_zig2(char *s);

    void *zig_variant(void *var);
}
