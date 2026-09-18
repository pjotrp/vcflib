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
- with --norm-multiallelic, all ALT alleles of multi-allelic input
  records are kept in the merged record (mirroring
  `bcftools norm -m-`); merged records then always satisfy the VCF
  spec and MULTI=ALTPROBLEM only marks true genotype conflicts
- the merged range is recorded in INFO combined=POS-POS

Differences from vcfcreatemulti:

- written in plain C (no C++ library, no zig compiler needed)
- non-overlapping records are passed through verbatim, without
  re-printing (so a '.' QUAL stays '.')

## Options

-h, --help

: shows help message and exits.

--norm-multiallelic

: keep all ALT alleles of multi-allelic input records in the merged
  record (mirrors `bcftools norm -m-`). Without this option the zig
  semantics drop those alleles from the merged ALT list while keeping
  their renumbered genotypes, which can produce records that violate
  the VCF spec (flagged MULTI=ALTPROBLEM). With it, merged records
  always satisfy the spec and MULTI=ALTPROBLEM only marks true
  genotype conflicts (a sample called for two different alleles
  across overlapping records).

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
    --norm-multiallelic keep all ALT alleles of multi-allelic input
                     records in the merged record (mirrors
                     `bcftools norm -m-`); merged records stay
                     valid, MULTI=ALTPROBLEM then only marks
                     true genotype conflicts
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

# VALIDATING GENERATED OUTPUT

The output of every regression run above is itself validated with
[vcfvalidate](./vcfvalidate.md). Outputs without MULTI=ALTPROBLEM
records must be fully valid:

```

>>> sh("for n in 2 3 4 5 6; do vcfvalidate -q ../test/tmp/vcfputtogetheragain_$n.vcf || echo FAILED $n; done; echo ALL_VALID")
ALL_VALID

```

Outputs with MULTI=ALTPROBLEM records are a documented caveat (the
zig merge semantics drop the multi-allelic record's ALT alleles but
keep its renumbered genotypes, so such records can carry allele
indices beyond the ALT count). We assert that every validation error
sits on a MULTI=ALTPROBLEM record:

```

>>> sh("vcfvalidate ../test/tmp/vcfputtogetheragain_7.vcf 2>&1 >/dev/null | grep '^  line:' | grep -vc MULTI=ALTPROBLEM")
0

>>> sh("vcfvalidate ../test/tmp/vcfputtogetheragain_8.vcf 2>&1 >/dev/null | grep '^  line:' | grep -vc MULTI=ALTPROBLEM")
0

```

# LICENSE

Copyright 2026 (C) Erik Garrison, Pjotr Prins and vcflib contributors. MIT licensed.
