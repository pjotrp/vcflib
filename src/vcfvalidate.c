/*
    vcfvalidate - VCFv4.5 data line validator.

    Copyright 2025 vcflib contributors. MIT licensed.

    Checks every data line of a VCF file against the field rules of the
    VCFv4.5 specification (see src/vcf-std.c) and reports descriptive,
    spec-referencing errors. Meta-information lines (##...) are not
    validated (deliberate scope decision, same as vcf-std).

    Exit status: 0 when all records are valid, 1 when at least one
    error was found, 2 on usage/file errors.

    Type: diagnostic
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vcf-std.h"

static void usage(void) {
    fprintf(stderr,
            "\nUsage: vcfvalidate [options] [file]\n\n"
            "Validate VCF data lines against the VCFv4.5 specification and report\n"
            "descriptive errors. Meta-information lines are not validated.\n\n"
            "options:\n\n"
            "    -h, --help       this help\n"
            "    -q, --quiet      suppress the summary line\n\n"
            "Exit status: 0 all records valid, 1 validation errors found.\n\n"
            "Type: diagnostic\n");
    exit(2);
}

int main(int argc, char **argv) {
    FILE *fp = stdin;
    int quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            usage();
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
            quiet = 1;
        else if (argv[i][0] == '-' && argv[i][1] != 0) {
            fprintf(stderr, "vcfvalidate: unknown option %s\n", argv[i]);
            usage();
        } else {
            fp = fopen(argv[i], "r");
            if (!fp) { perror(argv[i]); return 2; }
        }
    }

    char *buf = NULL;
    size_t cap = 0;
    ssize_t len;
    long lineno = 0, records = 0, errors = 0;

    while ((len = getline(&buf, &cap, fp)) != -1) {
        lineno++;
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r')
            continue;   /* header/meta lines and blanks are not validated */
        records++;

        vcfstd_error err;
        if (vcfstd_validate_record(buf, &err) != VCFSTD_OK) {
            errors++;
            char msg[512];
            vcfstd_error_string(&err, msg, sizeof(msg));
            fprintf(stderr, "vcfvalidate: line %ld: %s\n  line: %s", lineno, msg, buf);
        }
    }

    if (records == 0) {
        fprintf(stderr,
                "vcfvalidate: no data records found - file is empty or contains only header lines\n");
        free(buf);
        if (fp != stdin) fclose(fp);
        return 1;
    }

    if (!quiet)
        fprintf(stderr,
                "vcfvalidate: checked %ld records, found %ld error(s)\n",
                records, errors);

    free(buf);
    if (fp != stdin) fclose(fp);
    return errors ? 1 : 0;
}
