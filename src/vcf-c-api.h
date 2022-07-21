/*
  C API for vcflib
 */

extern "C" {

    void *zig_variant_window();
    void zig_variant_window_cleanup(void *varwin);
    void win_push(void *varwin, void *var);
    long win_size(void *varwin);

    char *hello_zig2(char *s);

    void *zig_variant(void *var);
}
