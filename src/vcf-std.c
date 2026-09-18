/*
    vcf-std.c - implementation of VCFv4.5 field validation.
    See vcf-std.h for the API and scope notes.
*/

#include "vcf-std.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 1024

static void fail(vcfstd_error *err, int field, int offset, const char *fmt, ...) {
    if (!err) return;
    va_list ap;
    err->code = VCFSTD_ERR;
    err->field = field;
    err->offset = offset;
    va_start(ap, fmt);
    vsnprintf(err->msg, sizeof(err->msg), fmt, ap);
    va_end(ap);
}

static void ok(vcfstd_error *err) {
    if (err) { err->code = VCFSTD_OK; err->field = -1; err->offset = -1; err->msg[0] = 0; }
}

void vcfstd_error_string(const vcfstd_error *err, char *out, size_t n) {
    if (!err || err->code == VCFSTD_OK) { snprintf(out, n, "no error"); return; }
    if (err->field >= 0)
        snprintf(out, n, "field %d: %s", err->field, err->msg);
    else
        snprintf(out, n, "%s", err->msg);
}

/* ------------------------------------------------------------------ */
/* shared helpers */

static int is_base_char(char c) {
    return strchr("ACGTNacgtn", c) != NULL;
}

/* control characters are disallowed (VCFv4.5 section 1.3: characters
   U+0000-U+0008, U+000B-U+000C, U+000E-U+001F; we reject all < 0x20
   except tab, which is the field delimiter) */
static int check_no_controls(const char *s, vcfstd_error *err, const char *what, int field) {
    for (const char *p = s; *p; p++) {
        if ((unsigned char)*p < 0x20 && *p != '\t') {
            fail(err, field, (int)(p - s),
                 "%s: control character 0x%02X not allowed (VCFv4.5 section 1.3)", what, *p);
            return VCFSTD_ERR;
        }
    }
    return VCFSTD_OK;
}

/* percent encoding: every '%' must be followed by two hex digits
   (VCFv4.5 section 1.5) */
static int check_percent_encoding(const char *s, vcfstd_error *err, const char *what, int field) {
    for (const char *p = s; *p; p++) {
        if (*p == '%') {
            if (!isxdigit((unsigned char)p[1]) || !isxdigit((unsigned char)p[2])) {
                fail(err, field, (int)(p - s),
                     "%s: bare '%%' - percent encoding expects two hex digits "
                     "(e.g. %%3A for ':'), see VCFv4.5 section 1.5", what);
                return VCFSTD_ERR;
            }
            p += 2;
        }
    }
    return VCFSTD_OK;
}

/* ------------------------------------------------------------------ */
/* fixed field validators */

/* CHROM: String, no whitespace; also accepts the angle-bracketed
   <ID> contig reference (VCFv4.5 section 1.4.1) */
int vcfstd_check_chrom(const char *s, vcfstd_error *err) {
    const int field = 0;
    if (!*s) { fail(err, field, 0, "CHROM: empty field (required, VCFv4.5 section 1.4.1)"); return VCFSTD_ERR; }
    if (check_no_controls(s, err, "CHROM", field)) return VCFSTD_ERR;
    for (const char *p = s; *p; p++) {
        if (isspace((unsigned char)*p)) {
            fail(err, field, (int)(p - s), "CHROM: whitespace not permitted (VCFv4.5 section 1.4.1)");
            return VCFSTD_ERR;
        }
    }
    return VCFSTD_OK;
}

/* POS: Integer >= 0 (0 and N+1 are telomere positions, section 1.4.2) */
int vcfstd_check_pos(const char *s, vcfstd_error *err) {
    const int field = 1;
    if (!*s) { fail(err, field, 0, "POS: empty field (required integer, VCFv4.5 section 1.4.2)"); return VCFSTD_ERR; }
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            fail(err, field, (int)(p - s),
                 "POS: '%c' is not a digit - POS must be a non-negative integer (VCFv4.5 section 1.4.2)", *p);
            return VCFSTD_ERR;
        }
    }
    return VCFSTD_OK;
}

/* ID: '.' or semicolon-separated list of unique identifiers, no
   whitespace or semicolons in the identifiers themselves (section 1.4.3) */
int vcfstd_check_id(const char *s, vcfstd_error *err) {
    const int field = 2;
    if (!*s) { fail(err, field, 0, "ID: empty field (use '.' for missing, VCFv4.5 section 1.4.3)"); return VCFSTD_ERR; }
    if (check_no_controls(s, err, "ID", field)) return VCFSTD_ERR;
    if (strcmp(s, ".") == 0) return VCFSTD_OK;

    const char *p = s;
    while (1) {
        const char *semi = strchr(p, ';');
        size_t len = semi ? (size_t)(semi - p) : strlen(p);
        if (len == 0) {
            fail(err, field, (int)(p - s), "ID: empty identifier (VCFv4.5 section 1.4.3)");
            return VCFSTD_ERR;
        }
        for (size_t i = 0; i < len; i++) {
            if (isspace((unsigned char)p[i])) {
                fail(err, field, (int)(p - s + i),
                     "ID: whitespace not permitted in identifier '%.*s' (VCFv4.5 section 1.4.3)",
                     (int)len, p);
                return VCFSTD_ERR;
            }
        }
        /* duplicate check against earlier identifiers */
        for (const char *q = s; q < p; ) {
            const char *qsemi = strchr(q, ';');
            size_t qlen = qsemi ? (size_t)(qsemi - q) : strlen(q);
            if (qlen == len && strncmp(q, p, len) == 0) {
                fail(err, field, (int)(p - s),
                     "ID: duplicate identifier '%.*s' (VCFv4.5 section 1.4.3: duplicate values not allowed)",
                     (int)len, p);
                return VCFSTD_ERR;
            }
            if (!qsemi) break;
            q = qsemi + 1;
        }
        if (!semi) break;
        p = semi + 1;
    }
    return VCFSTD_OK;
}

/* REF: non-empty string of bases A,C,G,T,N (case insensitive), section 1.4.4 */
int vcfstd_check_ref(const char *s, vcfstd_error *err) {
    const int field = 3;
    if (!*s) { fail(err, field, 0, "REF: empty field (required, VCFv4.5 section 1.4.4)"); return VCFSTD_ERR; }
    for (const char *p = s; *p; p++) {
        if (!is_base_char(*p)) {
            fail(err, field, (int)(p - s),
                 "REF: invalid base '%c' at offset %d - each base must be one of A,C,G,T,N "
                 "(case insensitive, VCFv4.5 section 1.4.4)", *p, (int)(p - s));
            return VCFSTD_ERR;
        }
    }
    return VCFSTD_OK;
}

/* one ALT allele (section 1.4.5): bases, '*', '.', symbolic <ID>,
   unspecified <NON_REF>/<*> style angle alleles, or breakend */
static int check_one_alt(const char *s, size_t len, int field, vcfstd_error *err) {
    if (len == 0) {
        fail(err, field, 0, "ALT: empty allele (use '.' for no variant, VCFv4.5 section 1.4.5)");
        return VCFSTD_ERR;
    }
    if (len == 1 && (s[0] == '.' || s[0] == '*')) return VCFSTD_OK;

    if (s[0] == '<') {
        if (len < 3 || s[len - 1] != '>') {
            fail(err, field, 0, "ALT: symbolic allele '%.*s' must be of the form <ID> (VCFv4.5 section 1.4.5)",
                 (int)len, s);
            return VCFSTD_ERR;
        }
        for (size_t i = 1; i < len - 1; i++) {
            char c = s[i];
            if (isspace((unsigned char)c) || c == ',' || c == '<' || c == '>') {
                fail(err, field, (int)i,
                     "ALT: whitespace, comma or angle bracket not permitted inside symbolic allele ID "
                     "'%.*s' (VCFv4.5 section 1.4.5)", (int)len, s);
                return VCFSTD_ERR;
            }
        }
        return VCFSTD_OK;
    }

    if (memchr(s, '[', len) || memchr(s, ']', len)) {
        /* breakend replacement string (section 5.4): loosely validated -
           must not contain whitespace, '<' or '>' */
        for (size_t i = 0; i < len; i++) {
            char c = s[i];
            if (isspace((unsigned char)c) || c == '<' || c == '>') {
                fail(err, field, (int)i,
                     "ALT: whitespace or angle brackets not permitted in breakend allele '%.*s' "
                     "(VCFv4.5 section 5.4)", (int)len, s);
                return VCFSTD_ERR;
            }
        }
        return VCFSTD_OK;
    }

    for (size_t i = 0; i < len; i++) {
        if (!is_base_char(s[i])) {
            fail(err, field, (int)i,
                 "ALT: invalid base '%c' at offset %d - each allele must be bases (A,C,G,T,N), '.', '*', "
                 "symbolic <ID> or a breakend (VCFv4.5 section 1.4.5)", s[i], (int)i);
            return VCFSTD_ERR;
        }
    }
    return VCFSTD_OK;
}

/* ALT: comma-separated list (section 1.4.5); returns VCFSTD_ERR also
   when the field is empty */
int vcfstd_check_alt(const char *s, vcfstd_error *err) {
    const int field = 4;
    if (!*s) { fail(err, field, 0, "ALT: empty field (use '.' for no variant, VCFv4.5 section 1.4.5)"); return VCFSTD_ERR; }
    if (check_no_controls(s, err, "ALT", field)) return VCFSTD_ERR;
    const char *p = s;
    while (1) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        int rc = check_one_alt(p, len, field, err);
        if (rc != VCFSTD_OK) return rc;
        if (!comma) break;
        p = comma + 1;
    }
    return VCFSTD_OK;
}

int vcfstd_count_alt(const char *alt_field) {
    if (!alt_field || !*alt_field) return -1;
    if (strcmp(alt_field, ".") == 0) return 0;
    int n = 1;
    for (const char *p = alt_field; *p; p++)
        if (*p == ',') n++;
    return n;
}

/* QUAL: '.' or a VCF Float (section 1.3: ^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$
   or INF/INFINITY/NAN, case insensitive) */
int vcfstd_check_qual(const char *s, vcfstd_error *err) {
    const int field = 5;
    if (!*s) { fail(err, field, 0, "QUAL: empty field (use '.' for missing, VCFv4.5 section 1.4.6)"); return VCFSTD_ERR; }
    if (strcmp(s, ".") == 0) return VCFSTD_OK;

    /* INF / INFINITY / NAN, case insensitive (section 1.3) */
    char low[16];
    size_t len = strlen(s);
    if (len < sizeof(low)) {
        for (size_t i = 0; i <= len; i++) low[i] = (char)tolower((unsigned char)s[i]);
        if (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0 || strcmp(low, "nan") == 0)
            return VCFSTD_OK;
    }

    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    int digits = 0, had_dot = 0, digits_after_dot = 0;
    while (isdigit((unsigned char)*p)) { p++; digits++; }
    if (*p == '.') {
        had_dot = 1;
        p++;
        while (isdigit((unsigned char)*p)) { p++; digits++; digits_after_dot++; }
    }
    /* VCF Float regexp ^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$ requires
       at least one digit in the mantissa, and if a '.' is present it
       must be followed by at least one digit (so "1." is invalid) */
    if (digits == 0 || (had_dot && digits_after_dot == 0)) {
        fail(err, field, 0,
             "QUAL: '%s' is not a valid VCF Float or '.' (VCFv4.5 sections 1.3 and 1.4.6)", s);
        return VCFSTD_ERR;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!isdigit((unsigned char)*p)) {
            fail(err, field, (int)(p - s),
                 "QUAL: '%s' is not a valid VCF Float (VCFv4.5 section 1.3)", s);
            return VCFSTD_ERR;
        }
        while (isdigit((unsigned char)*p)) p++;
    }
    if (*p != 0) {
        fail(err, field, (int)(p - s),
             "QUAL: '%s' is not a valid VCF Float or '.' (VCFv4.5 sections 1.3 and 1.4.6)", s);
        return VCFSTD_ERR;
    }
    return VCFSTD_OK;
}

/* FILTER: '.' or PASS or semicolon-separated unique codes, '0'
   reserved, no whitespace (section 1.4.7) */
int vcfstd_check_filter(const char *s, vcfstd_error *err) {
    const int field = 6;
    if (!*s) { fail(err, field, 0, "FILTER: empty field (use '.' if filters were not applied, VCFv4.5 section 1.4.7)"); return VCFSTD_ERR; }
    if (check_no_controls(s, err, "FILTER", field)) return VCFSTD_ERR;
    if (strcmp(s, ".") == 0 || strcmp(s, "PASS") == 0) return VCFSTD_OK;

    const char *p = s;
    while (1) {
        const char *semi = strchr(p, ';');
        size_t len = semi ? (size_t)(semi - p) : strlen(p);
        if (len == 0) {
            fail(err, field, (int)(p - s), "FILTER: empty filter code (VCFv4.5 section 1.4.7)");
            return VCFSTD_ERR;
        }
        if (len == 1 && p[0] == '0') {
            fail(err, field, (int)(p - s), "FILTER: '0' is reserved and must not be used (VCFv4.5 section 1.4.7)");
            return VCFSTD_ERR;
        }
        if (len == 4 && strncmp(p, "PASS", 4) == 0) {
            fail(err, field, (int)(p - s),
                 "FILTER: PASS must be used alone, not in a list (VCFv4.5 section 1.4.7)");
            return VCFSTD_ERR;
        }
        for (size_t i = 0; i < len; i++) {
            if (isspace((unsigned char)p[i])) {
                fail(err, field, (int)(p - s + i),
                     "FILTER: whitespace not permitted in filter code '%.*s' (VCFv4.5 section 1.4.7)",
                     (int)len, p);
                return VCFSTD_ERR;
            }
        }
        /* duplicates */
        for (const char *q = s; q < p; ) {
            const char *qsemi = strchr(q, ';');
            size_t qlen = qsemi ? (size_t)(qsemi - q) : strlen(q);
            if (qlen == len && strncmp(q, p, len) == 0) {
                fail(err, field, (int)(p - s),
                     "FILTER: duplicate filter code '%.*s' (VCFv4.5 section 1.4.7)", (int)len, p);
                return VCFSTD_ERR;
            }
            if (!qsemi) break;
            q = qsemi + 1;
        }
        if (!semi) break;
        p = semi + 1;
    }
    return VCFSTD_OK;
}

/* INFO key: ^([A-Za-z_][0-9A-Za-z_.]*|1000G)$ (section 1.4.8) */
static int valid_info_key(const char *s, size_t len) {
    if (len == 5 && strncmp(s, "1000G", 5) == 0) return 1;  /* legacy special */
    if (len == 0) return 0;
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) return 0;
    for (size_t i = 1; i < len; i++) {
        char c = s[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.')) return 0;
    }
    return 1;
}

/* INFO: '.' or semicolon-separated key[=value[,value...]]; keys must
   match the INFO key regexp, no duplicates; FLAG entries carry no '=';
   values may contain spaces; special characters must be percent encoded
   (sections 1.4.8 and 1.5) */
int vcfstd_check_info(const char *s, vcfstd_error *err) {
    const int field = 7;
    if (!*s) { fail(err, field, 0, "INFO: empty field (use '.' if no info present, VCFv4.5 section 1.4.8)"); return VCFSTD_ERR; }
    if (check_no_controls(s, err, "INFO", field)) return VCFSTD_ERR;
    if (strcmp(s, ".") == 0) return VCFSTD_OK;

    const char *p = s;
    while (1) {
        const char *semi = strchr(p, ';');
        size_t len = semi ? (size_t)(semi - p) : strlen(p);
        const char *eq = memchr(p, '=', len);
        size_t klen = eq ? (size_t)(eq - p) : len;
        if (klen == 0) {
            fail(err, field, (int)(p - s), "INFO: empty key (VCFv4.5 section 1.4.8)");
            return VCFSTD_ERR;
        }
        if (!valid_info_key(p, klen)) {
            fail(err, field, (int)(p - s),
                 "INFO: invalid key '%.*s' - keys must match ^([A-Za-z_][0-9A-Za-z_.]*|1000G)$ "
                 "(VCFv4.5 section 1.4.8)", (int)klen, p);
            return VCFSTD_ERR;
        }
        /* duplicate keys */
        for (const char *q = s; q < p; ) {
            const char *qsemi = strchr(q, ';');
            size_t qlen = qsemi ? (size_t)(qsemi - q) : strlen(q);
            const char *qeq = memchr(q, '=', qlen);
            size_t qklen = qeq ? (size_t)(qeq - q) : qlen;
            if (qklen == klen && strncmp(q, p, klen) == 0) {
                fail(err, field, (int)(p - s),
                     "INFO: duplicate key '%.*s' (VCFv4.5 section 1.4.8: duplicate keys not allowed)",
                     (int)klen, p);
                return VCFSTD_ERR;
            }
            if (!qsemi) break;
            q = qsemi + 1;
        }
        if (eq) {
            if ((size_t)(eq - p) + 1 >= len) {
                fail(err, field, (int)(p - s),
                     "INFO: key '%.*s' has an empty value (format is key=value, VCFv4.5 section 1.4.8)",
                     (int)klen, p);
                return VCFSTD_ERR;
            }
            /* literal ';' and '=' are not permitted in values; ',' only
               as list delimiter. Values are delimited by the split, so
               only '=' can still appear inside the value part. */
            for (const char *v = eq + 1; v < p + len; v++) {
                if (*v == '=') {
                    fail(err, field, (int)(v - s),
                         "INFO: literal '=' inside value - percent-encode as %%3D (VCFv4.5 section 1.5)");
                    return VCFSTD_ERR;
                }
            }
            const char *vend = p + len;
            const char *v = eq + 1;
            char *tmp = malloc(vend - v + 1);
            memcpy(tmp, v, vend - v);
            tmp[vend - v] = 0;
            int rc = check_percent_encoding(tmp, err, "INFO value", field);
            free(tmp);
            if (rc != VCFSTD_OK) return rc;
        }
        if (!semi) break;
        p = semi + 1;
    }
    return VCFSTD_OK;
}

/* ------------------------------------------------------------------ */
/* genotype fields (section 1.6.1) */

/* FORMAT: '.' or colon-separated keys ^[A-Za-z_][0-9A-Za-z_.]*$, no
   duplicates; GT must be the first key if present */
int vcfstd_check_format(const char *s, vcfstd_error *err) {
    const int field = 8;
    if (!*s) { fail(err, field, 0, "FORMAT: empty field (use '.' when there are no samples)"); return VCFSTD_ERR; }
    if (check_no_controls(s, err, "FORMAT", field)) return VCFSTD_ERR;
    if (strcmp(s, ".") == 0) return VCFSTD_OK;

    int gt_seen = 0;
    const char *p = s;
    int idx = 0;
    while (1) {
        const char *colon = strchr(p, ':');
        size_t len = colon ? (size_t)(colon - p) : strlen(p);
        if (len == 0) {
            fail(err, field, (int)(p - s), "FORMAT: empty key (VCFv4.5 section 1.6.1)");
            return VCFSTD_ERR;
        }
        if (!(isalpha((unsigned char)p[0]) || p[0] == '_')) {
            fail(err, field, (int)(p - s),
                 "FORMAT: key '%.*s' must match ^[A-Za-z_][0-9A-Za-z_.]*$ (VCFv4.5 section 1.6.1)",
                 (int)len, p);
            return VCFSTD_ERR;
        }
        for (size_t i = 1; i < len; i++) {
            char c = p[i];
            if (!(isalnum((unsigned char)c) || c == '_' || c == '.')) {
                fail(err, field, (int)(p - s + i),
                     "FORMAT: invalid character '%c' in key '%.*s' (VCFv4.5 section 1.6.1)", c, (int)len, p);
                return VCFSTD_ERR;
            }
        }
        if (len == 2 && strncmp(p, "GT", 2) == 0) {
            if (idx > 0) {
                fail(err, field, (int)(p - s),
                     "FORMAT: GT must be the first key when present (VCFv4.5 section 1.6.1)");
                return VCFSTD_ERR;
            }
            gt_seen = 1;
        }
        /* duplicates */
        for (const char *q = s; q < p; ) {
            const char *qcolon = strchr(q, ':');
            size_t qlen = qcolon ? (size_t)(qcolon - q) : strlen(q);
            if (qlen == len && strncmp(q, p, len) == 0) {
                fail(err, field, (int)(p - s),
                     "FORMAT: duplicate key '%.*s' (VCFv4.5 section 1.6.1)", (int)len, p);
                return VCFSTD_ERR;
            }
            if (!qcolon) break;
            q = qcolon + 1;
        }
        (void)gt_seen;
        if (!colon) break;
        p = colon + 1;
        idx++;
    }
    return VCFSTD_OK;
}

/* GT grammar: alleles '.' or integer indices in [0,nalt], separated by
   a consistent '/' (unphased) or '|' (phased); at least one allele
   (section 1.6.1 GT) */
int vcfstd_check_gt(const char *gt, int nalt, vcfstd_error *err) {
    if (!gt || !*gt) {
        fail(err, -1, 0, "GT: empty value (VCFv4.5 section 1.6.1)");
        return VCFSTD_ERR;
    }
    int sep = 0;              /* 0 = not seen yet, '/' or '|' */
    int nalleles = 0;
    const char *p = gt;
    while (1) {
        if (*p == '.') {
            p++;
        } else if (isdigit((unsigned char)*p)) {
            long idx = 0;
            while (isdigit((unsigned char)*p)) {
                idx = idx * 10 + (*p - '0');
                if (idx > 1000000) {
                    fail(err, -1, (int)(p - gt), "GT: allele index out of range (VCFv4.5 section 1.6.1)");
                    return VCFSTD_ERR;
                }
                p++;
            }
            if (idx > nalt) {
                fail(err, -1, 0,
                     "GT: allele index %ld out of range - only %d ALT allele(s) declared "
                     "(indices 0..%d valid, VCFv4.5 section 1.6.1)", idx, nalt, nalt);
                return VCFSTD_ERR;
            }
        } else {
            fail(err, -1, (int)(p - gt),
                 "GT: unexpected character '%c' - expected an allele index or '.' (VCFv4.5 section 1.6.1)",
                 *p ? *p : ' ');
            return VCFSTD_ERR;
        }
        nalleles++;
        if (*p == 0) break;
        if (*p != '/' && *p != '|') {
            fail(err, -1, (int)(p - gt),
                 "GT: unexpected character '%c' after allele (VCFv4.5 section 1.6.1)", *p);
            return VCFSTD_ERR;
        }
        if (sep == 0) sep = *p;
        else if (sep != *p) {
            fail(err, -1, (int)(p - gt),
                 "GT: mixed phased and unphased separators ('/' and '|') not allowed (VCFv4.5 section 1.6.1)");
            return VCFSTD_ERR;
        }
        p++;
        if (*p == 0) {
            fail(err, -1, (int)(p - gt),
                 "GT: trailing separator - expected an allele after '%c' (VCFv4.5 section 1.6.1)", sep);
            return VCFSTD_ERR;
        }
    }
    (void)nalleles;
    return VCFSTD_OK;
}


/* GT grammar check without the ALT range (used when the ALT count is
   not known at the sample level; the full-record path revalidates with
   the real count) */
static int check_gt_in_sample(const char *gt, vcfstd_error *err) {
    return vcfstd_check_gt(gt, 1000000, err);
}

/* split on ':' in place (helper for FORMAT/sample handling) */
static char **split_inplace_colon(char *s, int *count) {
    int cap = 8, n = 0;
    char **v = malloc(cap * sizeof(char *));
    char *p = s;
    v[n++] = p;
    while (*p) {
        if (*p == ':') {
            *p = 0;
            if (n == cap) { cap *= 2; v = realloc(v, cap * sizeof(char *)); }
            v[n++] = p + 1;
        }
        p++;
    }
    *count = n;
    return v;
}

/* one sample column against the FORMAT keys: colon-separated values,
   one per key; trailing fields may be dropped EXCEPT GT must be
   present if FORMAT declares it; missing values are '.'; empty
   subfields are not allowed (section 1.6.1) */
int vcfstd_check_sample(const char *format, const char *sample, vcfstd_error *err) {
    char *fmt = strdup(format);
    int nfk;
    char **fk = split_inplace_colon(fmt, &nfk);
    char *smp = strdup(sample);
    int nsv;
    char **sv = split_inplace_colon(smp, &nsv);

    int rc = VCFSTD_OK;
    if (nfk == 1 && strcmp(fk[0], ".") == 0) {
        fail(err, -1, 0, "sample column present but FORMAT is '.'");
        rc = VCFSTD_ERR;
        goto done;
    }
    if (nsv > nfk) {
        fail(err, -1, 0,
             "sample has %d value(s) but FORMAT declares %d key(s) (VCFv4.5 section 1.6.1)", nsv, nfk);
        rc = VCFSTD_ERR;
        goto done;
    }
    /* GT (first FORMAT key) must always be present - trailing fields
       may be dropped, but not GT (section 1.6.1). An empty first
       subfield counts as missing GT. */
    if (strcmp(fk[0], "GT") == 0 && (nsv == 0 || sv[0][0] == 0)) {
        fail(err, -1, 0,
             "sample column is empty but FORMAT declares GT - the GT field must always be present "
             "(VCFv4.5 section 1.6.1: only trailing fields can be dropped)");
        rc = VCFSTD_ERR;
        goto done;
    }
    for (int k = 0; k < nsv; k++) {
        if (sv[k][0] == 0) {
            fail(err, -1, 0,
                 "sample value %d is empty - use '.' for missing values (VCFv4.5 section 1.6.1)", k + 1);
            rc = VCFSTD_ERR;
            goto done;
        }
        if (strcmp(fk[k], "GT") == 0) {
            /* ALT count is checked by the caller via check_gt; here we
               only validate the grammar with a generous bound */
            rc = check_gt_in_sample(sv[k], err);
            if (rc != VCFSTD_OK) goto done;
        }
    }
done:
    free(fk);
    free(sv);
    free(fmt);
    free(smp);
    return rc;
}


/* ------------------------------------------------------------------ */
/* full record validation */

int vcfstd_validate_record_flags(const char *line, int level, vcfstd_error *err) {
    ok(err);
    const int deep = (level == VCFSTD_DEEP);

    /* work on a mutable copy so fields can be split in place; strip
       the trailing line separator (CR+LF or LF, section 1.3) */
    char *copy = strdup(line);
    if (!copy) { fail(err, -1, -1, "out of memory"); return VCFSTD_ERR; }
    size_t clen = strlen(copy);
    while (clen > 0 && (copy[clen - 1] == '\n' || copy[clen - 1] == '\r'))
        copy[--clen] = 0;
    const char **fields = malloc((MAX_FIELDS + 1) * sizeof(char *));
    if (!fields) { free(copy); fail(err, -1, -1, "out of memory"); return VCFSTD_ERR; }

    int nf = 0;
    char *p = copy;
    fields[nf++] = p;
    int bad_ctrl = 0;
    for (char *q = copy; *q; q++) {
        if ((unsigned char)*q < 0x20 && *q != '\t') {
            fail(err, -1, (int)(q - copy),
                 "control character 0x%02X not allowed (VCFv4.5 section 1.3)", *q);
            bad_ctrl = 1;
            break;
        }
        if (*q == '\t') {
            *q = 0;
            if (nf >= MAX_FIELDS) {
                fail(err, -1, -1, "record has too many fields (limit %d)", MAX_FIELDS);
                bad_ctrl = 1;
                break;
            }
            fields[nf++] = q + 1;
        }
    }
    int rc = VCFSTD_ERR;
    do {
        if (bad_ctrl) break;
        /* trailing tab would create an empty last field */
        if (nf > 1 && *(fields[nf - 1]) == 0) {
            fail(err, nf - 1, 0,
                 "trailing tab - data lines are tab-delimited with no tab at the end of the line "
                 "(VCFv4.5 section 1.4)");
            break;
        }
        if (nf < 8) {
            fail(err, nf, -1,
                 "record has %d field(s), expected at least the 8 fixed fields "
                 "CHROM,POS,ID,REF,ALT,QUAL,FILTER,INFO (VCFv4.5 section 1.4)", nf);
            break;
        }

        int (*checks[8])(const char *, vcfstd_error *) = {
            vcfstd_check_chrom, vcfstd_check_pos,    vcfstd_check_id,
            vcfstd_check_ref,   vcfstd_check_alt,    vcfstd_check_qual,
            vcfstd_check_filter, vcfstd_check_info,
        };
        int failed = 0;
        for (int i = 0; i < 8; i++) {
            if (checks[i](fields[i], err) != VCFSTD_OK) { failed = 1; break; }
        }
        if (failed) break;

        /* genotype columns */
        int nalt = vcfstd_count_alt(fields[4]);
        if (nalt == -1) {
            fail(err, 4, 0, "ALT: malformed field (VCFv4.5 section 1.4.5)");
            break;
        }
        if (nf == 9) {
            /* a FORMAT column with no sample data */
            fail(err, 8, 0, "FORMAT column present but no sample columns follow (VCFv4.5 section 1.6.1)");
            break;
        }
        if (nf > 9) {
            if (vcfstd_check_format(fields[8], err) != VCFSTD_OK) break;
            /* per-sample validation, allocation-free: walk each sample
               column against the FORMAT keys. GT is always the first
               FORMAT key when present (enforced by check_format), so
               the GT subfield is the first ':'-separated value. */
            int nfk = 1;
            for (const char *q = fields[8]; *q; q++)
                if (*q == ':') nfk++;
            int has_gt = (strncmp(fields[8], "GT", 2) == 0 &&
                          (fields[8][2] == 0 || fields[8][2] == ':'));
            int sample_failed = 0;
            for (int s = 9; s < nf; s++) {
                const char *v = fields[s];
                if (has_gt && v[0] == 0) {
                    if (err) { err->field = s; }
                    fail(err, s, 0,
                         "sample column is empty but FORMAT declares GT - the GT field must always be present "
                         "(VCFv4.5 section 1.6.1: only trailing fields can be dropped)");
                    sample_failed = 1;
                    break;
                }
                int idx = 0;
                const char *start = v;
                for (const char *q = v; ; q++) {
                    if (*q == ':' || *q == 0) {
                        size_t len = (size_t)(q - start);
                        if (len == 0) {
                            if (err) err->field = s;
                            fail(err, s, (int)(start - v),
                                 "sample value %d is empty - use '.' for missing values (VCFv4.5 section 1.6.1)",
                                 idx + 1);
                            sample_failed = 1;
                            break;
                        }
                        if (idx >= nfk) {
                            if (err) err->field = s;
                            fail(err, s, 0,
                                 "sample has more value(s) than the %d FORMAT key(s) declare "
                                 "(VCFv4.5 section 1.6.1)", nfk);
                            sample_failed = 1;
                            break;
                        }
                        if (idx == 0 && has_gt) {
                            /* GT: grammar always; the (expensive) allele
                               range check only in DEEP mode. The subfield
                               is ':'-separated, so terminate it
                               temporarily in our mutable copy. */
                            char saved = *q;
                            *(char *)q = 0;
                            int rc = vcfstd_check_gt(start, deep ? nalt : 1000000, err);
                            *(char *)q = saved;
                            if (rc != VCFSTD_OK) {
                                if (err) err->field = s;
                                sample_failed = 1;
                                break;
                            }
                        }
                        if (*q == 0) break;
                        idx++;
                        start = q + 1;
                    }
                }
                if (sample_failed) break;
            }
            if (sample_failed) break;
        }
        rc = VCFSTD_OK;
    } while (0);

    free(fields);
    free(copy);
    return rc;
}

/* convenience wrapper: full (DEEP) validation */
int vcfstd_validate_record(const char *line, vcfstd_error *err) {
    return vcfstd_validate_record_flags(line, VCFSTD_DEEP, err);
}
