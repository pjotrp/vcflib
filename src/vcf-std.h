/*
    vcf-std.h - VCF standard (samtools/hts-specs VCFv4.5) field parsing
    and validation. Dependency-free plain C; used by
    vcfputtogetheragain and exposed to Python via the pyvcfstd module.

    Validation is strict: any deviation from the specification is an
    error with a descriptive message. Meta-information lines (##...) are
    NOT validated by this library (deliberate scope decision).

    Spec references in messages cite VCFv4.5 sections:
      1.3    Data types
      1.5    Character encoding / percent encoding
      1.4    Data lines - fixed fields (CHROM/POS/ID/REF/ALT/QUAL/FILTER/INFO)
      1.6.1  Genotype fields (FORMAT keys, GT)
*/

#ifndef VCF_STD_H
#define VCF_STD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VCFSTD_OK  0
#define VCFSTD_ERR 1   /* any validation failure; see vcfstd_error.msg */

typedef struct {
    int  code;      /* VCFSTD_OK or VCFSTD_ERR */
    int  field;     /* 0-based field index in the record, -1 = n/a */
    int  offset;    /* byte offset within the field, -1 = n/a */
    char msg[256];  /* descriptive, human-readable message */
} vcfstd_error;

/* Validate one complete data line (no trailing newline). Checks the 8
   fixed fields, the FORMAT column and all sample columns. Returns
   VCFSTD_OK or VCFSTD_ERR with err filled in. */
int vcfstd_validate_record(const char *line, vcfstd_error *err);

/* Granular validators - exposed for testing and reuse. Each returns
   VCFSTD_OK or VCFSTD_ERR and fills err on failure. */
int vcfstd_check_chrom(const char *s, vcfstd_error *err);
int vcfstd_check_pos(const char *s, vcfstd_error *err);
int vcfstd_check_id(const char *s, vcfstd_error *err);
int vcfstd_check_ref(const char *s, vcfstd_error *err);
int vcfstd_check_alt(const char *s, vcfstd_error *err);   /* full ALT list */
int vcfstd_check_qual(const char *s, vcfstd_error *err);
int vcfstd_check_filter(const char *s, vcfstd_error *err);
int vcfstd_check_info(const char *s, vcfstd_error *err);
int vcfstd_check_format(const char *s, vcfstd_error *err); /* FORMAT column */
/* validate one sample column against the FORMAT keys */
int vcfstd_check_sample(const char *format, const char *sample, vcfstd_error *err);
/* validate a GT value against an ALT allele count (0 when ALT is '.') */
int vcfstd_check_gt(const char *gt, int nalt, vcfstd_error *err);

/* number of ALT alleles in an ALT field ('.' counts as 0); -1 if the
   field itself is malformed */
int vcfstd_count_alt(const char *alt_field);

/* fill a human-readable one-line summary: "error at field N: msg" */
void vcfstd_error_string(const vcfstd_error *err, char *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* VCF_STD_H */
