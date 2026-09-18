/*
    vcfputtogetheragain - plain C alternative to the zig implementation of
    vcfcreatemulti.

    Copyright 2025 vcflib contributors. MIT licensed.

    Go through a sorted VCF and when overlapping alleles are represented
    across multiple records, merge them into a single multi-ALT record -
    the "put Humpty Dumpty together again" companion to vcfwave. This is
    a reimplementation of the zig code path of vcfcreatemulti in plain C,
    without C++ or zig dependencies.

    ANNOTATION: correspondence with the original implementation
    ----------------------------------------------------------
    The zig version is split over three layers; this file follows the
    same decomposition. Map of this file to the original sources:

      C (this file)                     original
      ------------------------------    ---------------------------------
      GENOTYPE_MISSING, gt_parse,       src/zig/samples.zig
        gt_renumber, gt_merge,            GENOTYPE_MISSING, Genotypes
        gt_to_s                           (to_num/init/renumber/merge/to_s)
      sample_gt, sample_field           src/zig/vcf.zig Variant.genotypes
                                        + src/vcf-c-api.cpp var_geno
      expand_ref, expand_alt,           src/zig/vcf.zig
        expand_info                       expand_ref/expand_alt/expand_info
      print_merged                      src/vcfcreatemulti.cpp
                                          createMultiallelic_zig +
                                          zig_create_multi_allelic_inner
                                        + src/zig/vcf.zig
                                          zig_create_multi_allelic_inner
      main window loop                  src/vcfcreatemulti.cpp main()
      INFO output ordering              src/Variant.cpp Variant::write

    Deliberate differences from the zig code are marked with
    "DIFFERENCE:" in the comments below.

    Type: transformation
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vcf-std.h"

/* zig: samples.zig "const GENOTYPE_MISSING = -256;" - a sentinel that
   cannot collide with a real allele index */
#define GENOTYPE_MISSING (-256)

/* zig: vcf.zig keeps a global "warnings" StringHashMap so each warning
   is printed only once, from C++ via zig_display_warnings() at exit.
   The static flags below do the same deduplication. */
static int warned_multialt = 0;
static int warned_altproblem = 0;

/* zig: vcf.zig expand_alt() on a multi-allelic record:
       if (v.alt().items.len > 1) {
           warning("This code only supports one ALT allele per record: bailing out\n"
                   "Try normalising the data with `bcftools norm -m-`") catch unreachable;
           continue;
       }
   Same message, same single-shot deduplication. */
static void warn_multialt(void) {
    if (!warned_multialt) {
        warned_multialt = 1;
        fprintf(stderr,
                "WARNING: This code only supports one ALT allele per record: "
                "bailing out --- try normalising the data with `bcftools norm -m-`\n");
    }
}

/* zig: samples.zig Genotypes.merge() warning:
       try warning("Too many ALT alleles to fit in sample(s) - record marked with MULTI=ALTPROBLEM"); */
static void warn_altproblem(void) {
    if (!warned_altproblem) {
        warned_altproblem = 1;
        fprintf(stderr,
                "WARNING: Too many ALT alleles to fit in sample(s) - "
                "record marked with MULTI=ALTPROBLEM\n");
    }
}

/* ------------------------------------------------------------------ */
/* record: a VCF data line split into tab-separated fields (in place) */

typedef struct {
    char  *line;   /* owned buffer, split in place */
    char  *raw;    /* original, unsplit line (for pass-through) */
    char **f;      /* field pointers */
    int    nf;     /* number of fields */
    long   pos;    /* POS */
} Rec;

/* split s in place at sep; returns array of pointers into s */
static char **split_inplace(char *s, char sep, int *count) {
    int cap = 8, n = 0;
    char **v = malloc(cap * sizeof(char *));
    char *p = s;
    v[n++] = p;
    while (*p) {
        if (*p == sep) {
            *p = 0;
            if (n == cap) { cap *= 2; v = realloc(v, cap * sizeof(char *)); }
            v[n++] = p + 1;
        }
        p++;
    }
    *count = n;
    return v;
}

static Rec *rec_new(char *line) {
    Rec *r = calloc(1, sizeof(Rec));
    line[strcspn(line, "\r\n")] = 0;
    r->raw = strdup(line);
    r->line = line;
    r->f = split_inplace(line, '\t', &r->nf);
    r->pos = (r->nf > 1) ? atol(r->f[1]) : 0;
    return r;
}

static void rec_free(Rec *r) {
    free(r->f);
    free(r->line);
    free(r->raw);
    free(r);
}

/* ------------------------------------------------------------------ */
/* generic growable string */

typedef struct {
    char  *s;
    size_t len, cap;
} Buf;

static void buf_init(Buf *b) { b->cap = 64; b->len = 0; b->s = malloc(b->cap); b->s[0] = 0; }

static void buf_append_n(Buf *b, const char *s, size_t n) {
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;
}

static void buf_append(Buf *b, const char *s) { buf_append_n(b, s, strlen(s)); }

static void buf_append_char(Buf *b, char c) { buf_append_n(b, &c, 1); }

/* ------------------------------------------------------------------ */
/* INFO helpers */

/* get the value part of KEY=VALUE from an info string ("KEY=" or flag
   KEY returns an empty string); NULL if the key is absent */
static char *info_get_value(const char *info, const char *key) {
    size_t klen = strlen(key);
    char *tmp = strdup(info);
    int n;
    char **entries = split_inplace(tmp, ';', &n);
    char *res = NULL;
    for (int i = 0; i < n; i++) {
        if (strncmp(entries[i], key, klen) == 0) {
            if (entries[i][klen] == '=') {
                res = strdup(entries[i] + klen + 1);
            } else if (entries[i][klen] == 0) {
                res = strdup("");  /* flag */
            }
            break;
        }
    }
    free(entries);
    free(tmp);
    return res;
}

static int info_has_key(const char *info, const char *key) {
    char *v = info_get_value(info, key);
    if (!v) return 0;
    free(v);
    return 1;
}

/* ------------------------------------------------------------------ */
/* genotype helpers - a 1:1 port of the Genotypes struct in
   src/zig/samples.zig */

typedef struct {
    int *a;      /* allele indices, GENOTYPE_MISSING for '.' */
    int  n;
    int  phased;
} GT;

/* zig: samples.zig Genotypes.to_num() + init(): splits a GT string on
   '|' (phased) or '/' (unphased) into allele numbers.

   DIFFERENCE: zig indexes the first byte of each chunk unprotected -
   "if (chunk[0] == '.')" - which panicked with "index out of bounds:
   index 0, len 0" on an empty genotype string (this was the visible
   crash of the var_geno use-after-free bug, see git history). Our
   defensive patch treats an empty chunk as missing; here the same
   handling is built in for *p == 0. */
static GT gt_parse(const char *s) {
    GT g = {0};
    g.phased = strchr(s, '|') != NULL;
    int cap = 8;
    g.a = malloc(cap * sizeof(int));
    g.n = 0;
    const char *p = s;
    while (1) {
        int v;
        if (*p == 0 || *p == '.' || *p == '|' || *p == '/') {
            /* '.' or empty field: missing (zig: "if (chunk[0] == '.')
               GENOTYPE_MISSING else parseInt"); we also accept empty */
            v = GENOTYPE_MISSING;
            if (*p == '.') p++;
        } else {
            char *end;
            v = (int)strtol(p, &end, 10);
            p = end;
        }
        if (g.n == cap) { cap *= 2; g.a = realloc(g.a, cap * sizeof(int)); }
        g.a[g.n++] = v;
        if (*p == '|' || *p == '/') { p++; continue; }
        break;
    }
    return g;
}

/* zig: samples.zig Genotypes.renumber():
       fn renumber(self: *const Self, idx: usize) !void {
           var list = self.genos;
           for (list.items,0..) | g,i | {
               list.items[i] =
                   switch (g) {
                       0 => 0,
                       GENOTYPE_MISSING => GENOTYPE_MISSING,
                       else => g+@as(i64,@intCast(idx))
                   };
           }
       }
   Adds the variant's index in the window to every called allele, so
   allele 1 of window variant i points at merged ALT i+1. */
static void gt_renumber(GT *g, int idx) {
    for (int i = 0; i < g->n; i++) {
        if (g->a[i] != 0 && g->a[i] != GENOTYPE_MISSING)
            g->a[i] += idx;
    }
}

/* zig: samples.zig Genotypes.merge():
       for (genos2.genos.items,0..) | g2,i | {
           if (i >= base.items.len) break; // size mismatch guard
           const current = base.items[i];
           if (g2 == 0 or g2 == GENOTYPE_MISSING) continue; // no update
           if (current>0) {
               try warning("Too many ALT alleles to fit in sample(s) - record marked with MULTI=ALTPROBLEM");
               g_err = error.MultiAltSNPProblem;
           }
           base.items[i] = g2;
       }
   Note the zig conflict rule: it fires whenever both alleles are
   called (current>0), even if they are equal. We return 1 under the
   same condition; the caller turns it into MULTI=ALTPROBLEM. */
static int gt_merge(GT *base, const GT *g2) {
    int problem = 0;
    int n = base->n < g2->n ? base->n : g2->n;
    for (int i = 0; i < n; i++) {
        if (g2->a[i] == 0 || g2->a[i] == GENOTYPE_MISSING) continue;
        if (base->a[i] > 0) problem = 1;
        base->a[i] = g2->a[i];
    }
    return problem;
}

/* zig: samples.zig Genotypes.to_s(): joins alleles with the phase
   separator ('|' or '/'), printing GENOTYPE_MISSING as '.'. The zig
   version appends a separator after every allele and then drops the
   trailing one; here we simply join. */
static void gt_free(GT *g) { free(g->a); }

static char *gt_to_s(const GT *g) {
    Buf b;
    buf_init(&b);
    char tmp[16];
    for (int i = 0; i < g->n; i++) {
        if (i > 0) buf_append_char(&b, g->phased ? '|' : '/');
        if (g->a[i] == GENOTYPE_MISSING) buf_append_char(&b, '.');
        else { snprintf(tmp, sizeof(tmp), "%d", g->a[i]); buf_append(&b, tmp); }
    }
    return b.s;
}

/* zig: vcf.zig Variant.genotypes() calls into C++ via the C API:
       const size = var_samples_num(self.v);
       const buffer = allocator.alloc(*anyopaque, size);
       const res = var_geno(self.v, buffer);
       ...res[i] -> to_slice(s) -> list
   var_geno() (src/vcf-c-api.cpp) walks v->sampleNames and returns
   samples[sname]["GT"].front() for each sample.

   HISTORY: var_geno() used to take the samples map BY VALUE, so the
   returned char* pointers dangled after the call - the root cause of
   the scaffold612 crash. Fixed to a reference; here we read the GT
   field directly from the record line, so no such hazard exists.

   Missing GT (FORMAT without GT, or a short sample column) yields
   "." - the C API returns "." for an empty GT vector as well. */
static char *sample_gt(const Rec *r, int sample_idx) {
    if (r->nf < 10) return strdup(".");
    int nfk;
    char **fk = split_inplace(r->f[8], ':', &nfk);
    int gtidx = -1;
    for (int k = 0; k < nfk; k++)
        if (strcmp(fk[k], "GT") == 0) { gtidx = k; break; }
    free(fk);
    if (gtidx < 0) return strdup(".");
    int ns = r->nf - 9;
    if (sample_idx >= ns) return strdup(".");
    char *sf = strdup(r->f[9 + sample_idx]);
    int nsv;
    char **sv = split_inplace(sf, ':', &nsv);
    char *gt = strdup((gtidx < nsv && sv[gtidx][0]) ? sv[gtidx] : ".");
    free(sv);
    free(sf);
    return gt;
}

/* extract subfield `key` of a sample column (for output passthrough) */
static char *sample_field(const Rec *r, int sample_idx, const char *key) {
    if (r->nf < 10) return strdup(".");
    int nfk;
    char **fk = split_inplace(r->f[8], ':', &nfk);
    int idx = -1;
    for (int k = 0; k < nfk; k++)
        if (strcmp(fk[k], key) == 0) { idx = k; break; }
    free(fk);
    if (idx < 0) return strdup(".");
    int ns = r->nf - 9;
    if (sample_idx >= ns) return strdup(".");
    char *sf = strdup(r->f[9 + sample_idx]);
    int nsv;
    char **sv = split_inplace(sf, ':', &nsv);
    char *val = strdup((idx < nsv && sv[idx][0]) ? sv[idx] : ".");
    free(sv);
    free(sf);
    return val;
}

/* ------------------------------------------------------------------ */
/* merging - a 1:1 port of vcf.zig expand_ref/expand_alt/expand_info.
   zig operates on Variant structs that wrap C++ Variant objects via
   the C API; here records are plain split lines. */

/* zig: vcf.zig expand_ref():
       var res = ArrayList(u8){};
       res.appendSlice(allocator, first.ref());
       const left0 = first.pos();
       for (list.items) |v| {
           const right0 = left0 + res.items.len;   <- grows each round
           const left1 = v.pos();
           const right1 = left1 + v.ref().len;
           if (right1 > right0) {
               if (right0 >= left1) {
                   const sdiff = right1 - right0;
                   const pdiff = right0 - left1;
                   res.appendSlice(allocator, v.ref()[pdiff..pdiff+sdiff]);
               } else {
                   res.appendSlice(allocator, v.ref()); // non-overlapping
               }
           }
       }

           ref     sdiff
   ref0   |AAAAA|------->|
   ref1    |AAAAAAAAAAAAA|
          |--->| append |
           pdiff

   Note right0 is recomputed from the *growing* result each iteration,
   exactly as here (curlen = b.len). */
static char *expand_ref(Rec **rs, int n, size_t *outlen) {
    Buf b;
    buf_init(&b);
    buf_append(&b, rs[0]->f[3]);
    long left0 = rs[0]->pos;
    for (int i = 1; i < n; i++) {
        size_t curlen = b.len;
        long right0 = left0 + (long)curlen;
        long left1 = rs[i]->pos;
        long right1 = left1 + (long)strlen(rs[i]->f[3]);
        if (right1 > right0) {
            if (right0 >= left1) {
                long sdiff = right1 - right0;
                long pdiff = right0 - left1;
                buf_append_n(&b, rs[i]->f[3] + pdiff, (size_t)sdiff);
            } else {
                buf_append(&b, rs[i]->f[3]);  /* non-overlapping */
            }
        }
    }
    *outlen = b.len;
    return b.s;
}

/* zig: vcf.zig expand_alt(): splices each single-ALT allele into the
   expanded reference. zig's comments explain the three cases:

       // SNP       // Insertion      // Deletion
       //  ref       //  ref           //  ref
       // ref0 |AAAAAAAA|     ref0 |AAAAA|------->|   ref0 |AAAAA|
       // p5diff   p3diff=+3   p5diff   p3diff=-8      p5diff  p3diff=+2
       // SNP   C--->          ref1 |AAAAAAAAAAAAA|     ref1 |AA|--

   nalt = before + alt + after  (or just alt when p5==0 and p3==0).

   DIFFERENCE kept from zig: records with more than one ALT allele are
   skipped entirely (warn_multialt above) - their genotypes still
   participate in the merge, exactly as in the zig code. */
static char **expand_alt(Rec **rs, int n, const char *ref, size_t reflen, int *nout, int norm_multi) {
    /* in norm-multiallelic mode the array can grow beyond n records */
    char **alts = malloc((size_t)(norm_multi ? 4 * n + 4 : n) * sizeof(char *));
    int cap = norm_multi ? 4 * n + 4 : n;
    int cnt = 0;
    long pos0 = rs[0]->pos;
    for (int i = 0; i < n; i++) {
        if (rs[i]->pos < pos0) continue;
        int nalt;
        char **var_alts = split_inplace(rs[i]->f[4], ',', &nalt);
        if (nalt > 1 && !norm_multi) {  /* zig bails out on multi-allelic records */
            warn_multialt();
            free(var_alts);
            continue;
        }
        long p5 = rs[i]->pos - pos0;
        size_t before_len = (size_t)p5 < reflen ? (size_t)p5 : reflen;
        long right0 = pos0 + (long)reflen;
        long right1 = rs[i]->pos + (long)strlen(rs[i]->f[3]);
        long p3 = right0 - right1;
        const char *after = "";
        size_t after_len = 0;
        if (p3 > 0 && (size_t)p3 < reflen) {
            after = ref + reflen - (size_t)p3;
            after_len = (size_t)p3;
        }
        for (int k = 0; k < nalt; k++) {
            Buf t;
            buf_init(&t);
            if (p5 != 0 || p3 != 0) {
                buf_append_n(&t, ref, before_len);
                buf_append(&t, var_alts[k]);
                buf_append_n(&t, after, after_len);
            } else {
                buf_append(&t, var_alts[k]);
            }
            if (cnt == cap) { cap *= 2; alts = realloc(alts, (size_t)cap * sizeof(char *)); }
            alts[cnt++] = t.s;
        }
        free(var_alts);
    }
    *nout = cnt;
    return alts;
}

/* zig: vcf.zig expand_info():
       var ninfo = ArrayList([] const u8){};
       for (list.items) |v| {
           for (v.info(name).items) | info_item | {
               ninfo.append(allocator, info_item) catch unreachable;
           }
       }
   Concatenates the (already comma-split) INFO values of all records
   in window order; C++ writes them joined with ','. An empty result
   means the key is dropped from the output (C++ skips empty info
   vectors in Variant::write). */
static char *expand_info(Rec **rs, int n, const char *key) {
    Buf b;
    buf_init(&b);
    int any = 0;
    for (int i = 0; i < n; i++) {
        char *v = info_get_value(rs[i]->f[7], key);
        if (!v || v[0] == 0) { free(v); continue; }
        int np;
        char **pieces = split_inplace(v, ',', &np);
        for (int p = 0; p < np; p++) {
            if (any) buf_append_char(&b, ',');
            buf_append(&b, pieces[p]);
            any = 1;
        }
        free(pieces);
        free(v);
    }
    return b.s;  /* empty string when key absent everywhere */
}

/* ------------------------------------------------------------------ */
/* merged record output - the orchestration in
   vcf.zig zig_create_multi_allelic_inner():

       var nref = try expand_ref(Variant,vs);
       mvar.set_ref(c_nref);
       var nalt = try expand_alt(Variant,first.pos(),c_nref,vs);
       mvar.set_alt(nalt);
       const list = [_][] const u8{ "AN","AT","AC","AF","INV","TYPE" };
       for (list) |name| {
           var at = try expand_info(Variant,name,vs);
           mvar.set_info(name,at);
       }
       var genotypes_result = try samples.reduce_renumber_genotypes(Variant,vs);
       mvar.set_samples(genotypes_result.s_samples);
       if (genotypes_result.g_err != samples.VcfSampleError.None) {
           ninfo.append(allocator, "ALTPROBLEM") catch {};
           mvar.set_info("MULTI",ninfo);
       }

   mvar is the C++ copy of the first record (vcfcreatemulti.cpp
   createMultiallelic_zig: "Variant nvar = first"), so CHROM/ID/FILTER
   and all other fields come from record 0; the C++ caller then adds
   info["combined"] = "front.position-back.position". */
static void print_merged(Rec **rs, int n, int norm_multi) {
    if (n <= 0) return;
    /* vcfcreatemulti.cpp createMultiallelic_zig():
           if (vars.size() == 1) {
               return vars.front();      // pass through, no combined=
           }
       DIFFERENCE: the zig/C++ path re-prints the record through the
       C++ Variant writer (normalising QUAL '.' to 0, etc); we emit
       the original line untouched. */
    if (n == 1) {
        puts(rs[0]->raw);
        return;
    }

    Rec *first = rs[0];
    long pos0 = first->pos, posN = rs[n - 1]->pos;

    /* reference */
    size_t reflen;
    char *ref = expand_ref(rs, n, &reflen);

    /* alts */
    int nalt;
    char **alts = expand_alt(rs, n, ref, reflen, &nalt, norm_multi);

    /* genotype renumber offsets: zig parity renumbers by window record
       index; --norm-multiallelic renumbers by the cumulative number of
       ALT alleles contributed by the preceding records, so every allele
       index stays within the merged ALT list (mirrors
       `bcftools norm -m-`). */
    int *renum = malloc((size_t)n * sizeof(int));
    if (norm_multi) {
        int off = 0;
        for (int i = 0; i < n; i++) {
            renum[i] = off;
            for (const char *q = rs[i]->f[4]; *q; q++)
                if (*q == ',') off++;
            off++;
        }
    } else {
        for (int i = 0; i < n; i++) renum[i] = i;
    }

    /* genotypes: zig samples.reduce_renumber_genotypes():
           for (vs.items, 0..) | v,i | {
               for (v.genotypes().items, 0..) | geno,j | {
                   var geno2 = try Genotypes.init(geno);
                   try geno2.renumber(i);        <- offset = window index
                   if (i==0) sample_list.append(geno2)
                   else sample_list.items[j].merge(geno2)
               }
           }
       The base genotype comes from record 0; records i>0 are renumbered
       by i and merged in. Any conflict sets g_err -> MULTI=ALTPROBLEM. */
    int nsamples = first->nf - 9;
    if (nsamples < 0) nsamples = 0;
    char **sample_out = malloc((size_t)(nsamples > 0 ? nsamples : 1) * sizeof(char *));
    int problem = 0;
    for (int j = 0; j < nsamples; j++) {
        char *gt0 = sample_gt(first, j);
        GT base = gt_parse(gt0);
        free(gt0);
        for (int i = 1; i < n; i++) {
            if (rs[i]->nf - 9 <= j) break;  /* fewer samples in this record */
            char *gti = sample_gt(rs[i], j);
            GT g2 = gt_parse(gti);
            free(gti);
            gt_renumber(&g2, renum[i]);
            if (gt_merge(&base, &g2)) problem = 1;
            gt_free(&g2);
        }
        sample_out[j] = gt_to_s(&base);
        gt_free(&base);
    }

    /* INFO output. The zig code calls mvar.set_info(name, values) for
       AN,AT,AC,AF,INV,TYPE; C++ Variant::write (src/Variant.cpp) then
       prints keys in their original record order, followed by keys
       added after parsing (MULTI, combined, or merged keys absent from
       the first record) in ASCII sorted order - hence MULTI before
       combined. We reproduce that exact ordering here. */
    static const char *merge_keys[] = { "AN", "AT", "AC", "AF", "INV", "TYPE" };
    const int nmerge_keys = 6;
    char *merged[6];
    for (int k = 0; k < nmerge_keys; k++)
        merged[k] = expand_info(rs, n, merge_keys[k]);

    Buf info;
    buf_init(&info);
    int nent;
    char **entries = split_inplace(first->f[7], ';', &nent);
    int emitted = 0;
    int done_key[6] = {0, 0, 0, 0, 0, 0};
    for (int e = 0; e < nent; e++) {
        char *entry = entries[e];
        char *eq = strchr(entry, '=');
        size_t klen = eq ? (size_t)(eq - entry) : strlen(entry);
        int replaced = 0;
        for (int k = 0; k < nmerge_keys; k++) {
            if (strlen(merge_keys[k]) == klen && strncmp(entry, merge_keys[k], klen) == 0) {
                done_key[k] = 1;
                if (merged[k][0]) {
                    if (emitted++) buf_append_char(&info, ';');
                    buf_append(&info, merge_keys[k]);
                    buf_append_char(&info, '=');
                    buf_append(&info, merged[k]);
                }
                replaced = 1;
                break;
            }
        }
        if (!replaced) {
            if (emitted++) buf_append_char(&info, ';');
            buf_append(&info, entry);
        }
    }
    /* append merged keys that were not in the first record plus
       combined/MULTI, sorted by key (mirrors the C++ output code
       appending missing keys in sorted order) */
    Buf extra[8];
    const char *extra_key[8];
    int nextra = 0;
    char tmp[64];
    for (int k = 0; k < nmerge_keys; k++) {
        if (!done_key[k] && merged[k][0]) {
            buf_init(&extra[nextra]);
            buf_append(&extra[nextra], merge_keys[k]);
            buf_append_char(&extra[nextra], '=');
            buf_append(&extra[nextra], merged[k]);
            extra_key[nextra] = merge_keys[k];
            nextra++;
        }
    }
    buf_init(&extra[nextra]);
    snprintf(tmp, sizeof(tmp), "combined=%ld-%ld", pos0, posN);
    buf_append(&extra[nextra], tmp);
    extra_key[nextra] = "combined";
    nextra++;
    if (problem) {
        buf_init(&extra[nextra]);
        buf_append(&extra[nextra], "MULTI=ALTPROBLEM");
        extra_key[nextra] = "MULTI";
        nextra++;
    }
    /* insertion sort by key (ASCII) */
    for (int i = 1; i < nextra; i++) {
        Buf bv = extra[i]; const char *bk = extra_key[i];
        int j = i - 1;
        while (j >= 0 && strcmp(extra_key[j], bk) > 0) {
            extra[j + 1] = extra[j]; extra_key[j + 1] = extra_key[j]; j--;
        }
        extra[j + 1] = bv; extra_key[j + 1] = bk;
    }
    for (int i = 0; i < nextra; i++) {
        if (emitted++) buf_append_char(&info, ';');
        buf_append(&info, extra[i].s);
    }
    if (problem) warn_altproblem();
    for (int i = 0; i < nextra; i++) free(extra[i].s);

    /* output record */
    printf("%s\t%ld\t%s\t%s\t", first->f[0], pos0, first->f[2], ref);
    for (int a = 0; a < nalt; a++) printf("%s%s", a ? "," : "", alts[a]);
    /* QUAL/FILTER: zig leaves mvar's first-record fields in place, but
       the C++ Variant writer prints quality as a double ("." parses to
       0), which is why zig output shows 0 where the input has '.'.
       %g matches the default C++ ostream double formatting. */
    printf("\t%g\t%s\t%s", strtod(first->f[5], NULL), first->f[6], info.s);

    /* Sample columns: the zig code calls mvar.set_samples() which only
       replaces the GT field (var_set_sample pushes to
       samples[sname]["GT"]); all other FORMAT fields keep the values
       of the FIRST record - including the now possibly mismatched
       DS/GP values. Same behaviour here. */
    if (nsamples > 0 && first->nf > 8 && first->f[8][0]) {
        int nfk;
        char **fk = split_inplace(first->f[8], ':', &nfk);
        printf("\t%s", first->f[8]);
        for (int j = 0; j < nsamples; j++) {
            printf("\t");
            for (int k = 0; k < nfk; k++) {
                if (k) printf(":");
                if (strcmp(fk[k], "GT") == 0) {
                    printf("%s", sample_out[j]);
                } else {
                    char *v = sample_field(first, j, fk[k]);
                    printf("%s", v);
                    free(v);
                }
            }
        }
        free(fk);
    }
    printf("\n");

    /* cleanup */
    free(renum);
    free(ref);
    for (int a = 0; a < nalt; a++) free(alts[a]);
    free(alts);
    for (int j = 0; j < nsamples; j++) free(sample_out[j]);
    free(sample_out);
    free(info.s);
    free(entries);
    for (int k = 0; k < nmerge_keys; k++) free(merged[k]);
}

/* ------------------------------------------------------------------ */

static void usage(void) {
    fprintf(stderr,
            "\nUsage: vcfputtogetheragain [options] [file]\n\n"
            "Go through sorted VCF and when overlapping alleles are represented across multiple records, merge them into a single multi-ALT record. Plain C alternative to the zig implementation of vcfcreatemulti.\n\n"
            "options:\n\n"
            "    -h, --help       this help\n"
            "    --validate       run the expensive VCF standard checks\n"
            "                     (per-sample GT allele range validation)\n"
            "    --norm-multiallelic keep all ALT alleles of multi-allelic input\n"
            "                     records in the merged record (mirrors\n"
            "                     `bcftools norm -m-`); merged records stay\n"
            "                     valid, MULTI=ALTPROBLEM then only marks\n"
            "                     true genotype conflicts\n\n"
            "Type: transformation\n");
    exit(1);
}

/* Windowing - a port of the main() loop in vcfcreatemulti.cpp:

       auto first = vars.front();
       auto maxpos = first.position + first.ref.size();
       for (const auto& v: vars)
           if (maxpos < v.position + v.ref.size()) maxpos = ...;

       if (var.sequenceName != lastSeqName)   flush (next chromosome)
       else if (var.position < maxpos)        push (in window)
       else                                   flush (out of window)

   The window only ever grows: maxpos is recomputed from all records
   accumulated so far, so a long insertion keeps pulling later records
   into the same merge. The unsorted check (exit 8) is also from the
   C++ main loop. */
int main(int argc, char **argv) {
    FILE *fp = stdin;
    int level = VCFSTD_BASIC;   /* --validate enables the expensive checks */
    int norm_multi = 0;         /* --norm-multiallelic keeps all ALT alleles */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            usage();
        else if (strcmp(argv[i], "--validate") == 0)
            level = VCFSTD_DEEP;
        else if (strcmp(argv[i], "--norm-multiallelic") == 0)
            norm_multi = 1;
        else {
            fp = fopen(argv[i], "r");
            if (!fp) { perror(argv[i]); return 1; }
        }
    }

    char *buf = NULL;
    size_t cap = 0;
    ssize_t len;
    long lineno = 0;

    Rec **vars = NULL;
    int nvars = 0, capvars = 0;
    char *last_seq = NULL;

    while ((len = getline(&buf, &cap, fp)) != -1) {
        lineno++;
        /* header lines pass through; add the INFO definitions that
           vcfcreatemulti.cpp registers via variantFile.addHeaderLine() */
        if (len > 0 && (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r')) {
            /* insert the INFO header lines used by the merger */
            if (buf[0] == '#' && buf[1] == 'C') {
                printf("##INFO=<ID=combined,Number=1,Type=String,Description=\"Range of overlapping variants which were combined into this one using vcfputtogetheragain.\">\n");
                printf("##INFO=<ID=MULTI,Number=1,Type=String,Description=\"Identify problematic ALT reconstruction - see vcfcreatemulti.md doc.\">\n");
            }
            fputs(buf, stdout);
            continue;
        }
        /* strict VCFv4.5 field validation - a malformed line is a hard
           error; malformed data is never passed into the merger */
        vcfstd_error err;
        if (vcfstd_validate_record_flags(buf, level, &err) != VCFSTD_OK) {
            char msg[512];
            vcfstd_error_string(&err, msg, sizeof(msg));
            fprintf(stderr, "ERROR: invalid VCF data line %ld: %s\n  line: %s", lineno, msg, buf);
            return 1;
        }
        Rec *r = rec_new(strdup(buf));

        /* sortedness check (mirrors vcfcreatemulti) */
        if (nvars > 0 && strcmp(vars[nvars - 1]->f[0], r->f[0]) == 0 &&
            vars[nvars - 1]->pos > r->pos) {
            fprintf(stderr, "ERROR: VCF data is not sorted! at %s:%ld and %s:%ld\n",
                    vars[nvars - 1]->f[0], vars[nvars - 1]->pos, r->f[0], r->pos);
            exit(8);
        }

        if (nvars == 0) {
            free(last_seq);
            last_seq = strdup(r->f[0]);
        } else {
            /* maxpos = furthest reach of the current window */
            long maxpos = 0;
            for (int i = 0; i < nvars; i++) {
                long mp = vars[i]->pos + (long)strlen(vars[i]->f[3]);
                if (mp > maxpos) maxpos = mp;
            }
            int flush = 0;
            if (strcmp(last_seq, r->f[0]) != 0) {
                flush = 1;
            } else if (r->pos >= maxpos) {
                flush = 1;
            }
            if (flush) {
                print_merged(vars, nvars, norm_multi);
                for (int i = 0; i < nvars; i++) rec_free(vars[i]);
                nvars = 0;
                free(last_seq);
                last_seq = strdup(r->f[0]);
            }
        }
        if (nvars == capvars) {
            capvars = capvars ? capvars * 2 : 16;
            vars = realloc(vars, (size_t)capvars * sizeof(Rec *));
        }
        vars[nvars++] = r;
    }
    if (nvars > 0) {
        print_merged(vars, nvars, norm_multi);
        for (int i = 0; i < nvars; i++) rec_free(vars[i]);
    }
    free(vars);
    free(last_seq);
    free(buf);
    if (fp != stdin) fclose(fp);
    return 0;
}
