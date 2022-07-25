/*
  C API for vcflib
 */

// extern "C" {
    // Variant accessors
    const char *get_id(void *variant);
    void set_id(void *variant, const char *);
    extern void testme();

    // Some test functions
    void *zig_variant_window();
    void zig_variant_window_cleanup(void *varwin);
    void win_push(void *varwin, void *var);
    long win_size(void *varwin);

    void *zig_create_multi_allelic(void *retvar, void *varlist[], long size);

    char *hello_zig2(char *s);

    void *zig_variant(void *var);
// }
