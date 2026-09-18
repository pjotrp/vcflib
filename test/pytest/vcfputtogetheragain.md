% VCFPUTTOGETHERAGAIN(1) vcfputtogetheragain (vcflib) | vcfputtogetheragain (VCF transformation)
% Erik Garrison, Pjotr Prins and other vcflib contributors

# NAME

vcfputtogetheragain - collates single ALT allele records into multi-allele records while tracking genotypes (plain C)

# SYNOPSIS

**vcfputtogetheragain**

# DESCRIPTION

**vcfputtogetheragain** is a plain C alternative to the zig
implementation of [vcfcreatemulti](./vcfcreatemulti.md). It merges
VCF records into one line by combining ALT alleles into a single VCF
record - the "put Humpty Dumpty together again" companion to
[vcfwave](./vcfwave.md) - tracking INFO records and genotypes.

It replicates the semantics of the zig code path of vcfcreatemulti:

- the reference is expanded so it covers all overlapping variants
- ALT alleles are spliced into the expanded reference (multi-allelic
  input records are skipped with a warning, like the zig code)
- INFO values of AN,AT,AC,AF,INV,TYPE are concatenated
- sample genotypes are merged and allele indices renumbered; on
  conflicts the record is flagged with MULTI=ALTPROBLEM
- the merged range is recorded in INFO combined=POS-POS

Differences from vcfcreatemulti:

- written in plain C (no C++ library, no zig compiler needed)
- non-overlapping records are passed through verbatim, without
  re-printing (so a '.' QUAL stays '.')

## Options

-h, --help

: shows help message and exits.

See more below.

# EXIT VALUES

**0**
: Success

**not 0**
: Failure

# EXAMPLES


<!--

    >>> from rtest import run_stdout, head, cat, sh

-->

```

>>> head("vcfputtogetheragain -h",25)
>
Usage: vcfputtogetheragain [options] [file]
>
Go through sorted VCF and when overlapping alleles are represented across multiple records, merge them into a single multi-ALT record. Plain C alternative to the zig implementation of vcfcreatemulti.
>
options:
>
    -h, --help       this help
    --validate       run the expensive VCF standard checks
                     (per-sample GT allele range validation)
>
Type: transformation
>

```

The regression tests use the same inputs as
[vcfcreatemulti](./vcfcreatemulti.md) so output can be compared
one-to-one (modulo the tool name in the combined INFO header
description and the verbatim pass-through of non-overlapping
records):

```

>>> run_stdout("vcfputtogetheragain ../samples/sample.vcf", ext="vcf", uniq=2)
output in <a href="../data/regression/vcfputtogetheragain_2.vcf">vcfputtogetheragain_2.vcf</a>

>>> run_stdout("vcfputtogetheragain ../samples/test-dup.vcf", ext="vcf", uniq=3)
output in <a href="../data/regression/vcfputtogetheragain_3.vcf">vcfputtogetheragain_3.vcf</a>

>>> run_stdout("vcfputtogetheragain ../samples/test-multi.vcf", ext="vcf", uniq=4)
output in <a href="../data/regression/vcfputtogetheragain_4.vcf">vcfputtogetheragain_4.vcf</a>

>>> run_stdout("vcfputtogetheragain ../samples/10158243.vcf", ext="vcf", uniq=5)
output in <a href="../data/regression/vcfputtogetheragain_5.vcf">vcfputtogetheragain_5.vcf</a>

```

A minimalised record from
[vcflib issue 354](https://github.com/vcflib/vcflib/issues/354):

```

>>> run_stdout("vcfputtogetheragain ../test/data/inputs/issue354-type.vcf", ext="vcf", uniq=6)
output in <a href="../data/regression/vcfputtogetheragain_6.vcf">vcfputtogetheragain_6.vcf</a>

```

Regression tests for the use-after-free that used to crash the zig
implementation (fixed in the C API, see git history): overlapping
scaffold612 records and a larger extract of that region:

```

>>> run_stdout("vcfputtogetheragain ../test/data/inputs/issue-scaffold612-mini.vcf", ext="vcf", uniq=8)
output in <a href="../data/regression/vcfputtogetheragain_8.vcf">vcfputtogetheragain_8.vcf</a>

>>> run_stdout("vcfputtogetheragain ../test/data/inputs/scaffold612-uaf-region.vcf", ext="vcf", uniq=7)
output in <a href="../data/regression/vcfputtogetheragain_7.vcf">vcfputtogetheragain_7.vcf</a>

```

# LICENSE

Copyright 2025 (C) Erik Garrison, Pjotr Prins and vcflib contributors. MIT licensed.
