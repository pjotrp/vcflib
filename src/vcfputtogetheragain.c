/*
    vcfputtogetheragain - plain C alternative to the zig implementation of
    vcfcreatemulti.

    Copyright 2025 vcflib contributors. MIT licensed.

    Go through a sorted VCF and when overlapping alleles are represented
    across multiple records, merge them into a single multi-ALT record -
    the "put Humpty Dumpty together again" companion to vcfwave. This is
    a reimplementation of the zig code path of vcfcreatemulti
    (src/zig/vcf.zig, src/zig/samples.zig) in plain C, without C++ or
    zig dependencies:

    - the reference is expanded so it covers all overlapping variants
    - ALT alleles are spliced into the expanded reference
    - INFO values of AN,AT,AC,AF,INV,TYPE are concatenated
    - sample genotypes are merged and allele numbers renumbered; on
      conflicts the record is marked MULTI=ALTPROBLEM
    - the merged range is tracked in INFO combined=POS-POS

    Type: transformation
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GENOTYPE_MISSING (-256)

static int warned_multialt = 0;
static int warned_altproblem = 0;

static void warn_multialt(void) {
    if (!warned_multialt) {
        warned_multialt = 1;
        fprintf(stderr,
                "WARNING: This code only supports one ALT allele per record: "
                "bailing out --- try normalising the data with `bcftools norm -m-`\n");
    }
}

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
/* genotype helpers (mirrors src/zig/samples.zig) */

typedef struct {
    int *a;      /* allele indices, GENOTYPE_MISSING for '.' */
    int  n;
    int  phased;
} GT;

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
            /* empty field or '.': missing (zig panics on empty, we
               treat it as missing - see samples.zig to_num) */
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

/* add offset idx to all called alleles (0 and . unchanged) */
static void gt_renumber(GT *g, int idx) {
    for (int i = 0; i < g->n; i++) {
        if (g->a[i] != 0 && g->a[i] != GENOTYPE_MISSING)
            g->a[i] += idx;
    }
}

/* merge g2 into base; returns 1 on conflict (both called, different) */
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

/* extract the GT subfield of a sample column, given the FORMAT keys */
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
/* merging (mirrors vcf.zig expand_ref/expand_alt/expand_info) */

/* expand the reference so it covers all records in the window */
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

/* splice all (single-ALT) variant alleles into the expanded reference */
static char **expand_alt(Rec **rs, int n, const char *ref, size_t reflen, int *nout) {
    char **alts = malloc((size_t)n * sizeof(char *));
    int cnt = 0;
    long pos0 = rs[0]->pos;
    for (int i = 0; i < n; i++) {
        if (rs[i]->pos < pos0) continue;
        int nalt;
        char **var_alts = split_inplace(rs[i]->f[4], ',', &nalt);
        if (nalt > 1) {  /* zig bails out on multi-allelic records */
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
        Buf t;
        buf_init(&t);
        if (p5 != 0 || p3 != 0) {
            buf_append_n(&t, ref, before_len);
            buf_append(&t, var_alts[0]);
            buf_append_n(&t, after, after_len);
        } else {
            buf_append(&t, var_alts[0]);
        }
        alts[cnt++] = t.s;
        free(var_alts);
    }
    *nout = cnt;
    return alts;
}

/* collect and comma-join the INFO values of `key` over all records */
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
/* merged record output */

static void print_merged(Rec **rs, int n) {
    if (n <= 0) return;
    if (n == 1) {  /* pass through unchanged */
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
    char **alts = expand_alt(rs, n, ref, reflen, &nalt);

    /* genotypes: merge per sample, renumbering by variant index */
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
            gt_renumber(&g2, i);
            if (gt_merge(&base, &g2)) problem = 1;
            gt_free(&g2);
        }
        sample_out[j] = gt_to_s(&base);
        gt_free(&base);
    }

    /* INFO: original keys in order, merged values for AN,AT,AC,AF,INV,TYPE,
       then remaining merged keys sorted (ASCII), then combined/MULTI */
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
    printf("\t%g\t%s\t%s", strtod(first->f[5], NULL), first->f[6], info.s);

    /* sample columns: merged GT + the other FORMAT fields of the first
       record (the zig code only updates GT) */
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
            "    -h, --help       this help\n\n"
            "Type: transformation\n");
    exit(1);
}

int main(int argc, char **argv) {
    FILE *fp = stdin;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            usage();
        else {
            fp = fopen(argv[i], "r");
            if (!fp) { perror(argv[i]); return 1; }
        }
    }

    char *buf = NULL;
    size_t cap = 0;
    ssize_t len;

    Rec **vars = NULL;
    int nvars = 0, capvars = 0;
    char *last_seq = NULL;

    while ((len = getline(&buf, &cap, fp)) != -1) {
        if (len > 0 && (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r')) {
            /* insert the INFO header lines used by the merger */
            if (buf[0] == '#' && buf[1] == 'C') {
                printf("##INFO=<ID=combined,Number=1,Type=String,Description=\"Range of overlapping variants which were combined into this one using vcfputtogetheragain.\">\n");
                printf("##INFO=<ID=MULTI,Number=1,Type=String,Description=\"Identify problematic ALT reconstruction - see vcfcreatemulti.md doc.\">\n");
            }
            fputs(buf, stdout);
            continue;
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
                print_merged(vars, nvars);
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
        print_merged(vars, nvars);
        for (int i = 0; i < nvars; i++) rec_free(vars[i]);
    }
    free(vars);
    free(last_seq);
    free(buf);
    if (fp != stdin) fclose(fp);
    return 0;
}
