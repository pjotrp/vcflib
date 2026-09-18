% VCFPUTTOGETHERAGAIN-NORM(1) vcfputtogetheragain --norm-multiallelic | VCF transformation
% Pjotr Prins and vcflib contributors

# NAME

vcfputtogetheragain --norm-multiallelic - merge overlapping records
keeping all ALT alleles of multi-allelic input records

# DESCRIPTION

These tests run [vcfputtogetheragain](./vcfputtogetheragain.md) with
the **--norm-multiallelic** switch, which mirrors `bcftools norm -m-`:
all ALT alleles of multi-allelic input records are kept in the merged
record, so every allele index in the merged genotypes is valid and
MULTI=ALTPROBLEM only marks true genotype conflicts. The default mode
follows the zig semantics of vcfcreatemulti (see
[vcfputtogetheragain.md](./vcfputtogetheragain.md) for the comparison).

<!--

    >>> from rtest import run_stdout, head, cat, sh

-->

# EXAMPLES

The scaffold612 region that triggers the default mode's ALTPROBLEM
caveat: a SNP and a multi-allelic insertion at the same position.

```

>>> run_stdout("vcfputtogetheragain --norm-multiallelic ../test/data/inputs/issue-scaffold612-mini.vcf", ext="vcf", uniq=2)
output in <a href="../data/regression/vcfputtogetheragain-norm_2.vcf">vcfputtogetheragain-norm_2.vcf</a>

>>> run_stdout("vcfputtogetheragain --norm-multiallelic ../test/data/inputs/scaffold612-uaf-region.vcf", ext="vcf", uniq=3)
output in <a href="../data/regression/vcfputtogetheragain-norm_3.vcf">vcfputtogetheragain-norm_3.vcf</a>

>>> run_stdout("vcfputtogetheragain --norm-multiallelic ../samples/test-dup.vcf", ext="vcf", uniq=4)
output in <a href="../data/regression/vcfputtogetheragain-norm_4.vcf">vcfputtogetheragain-norm_4.vcf</a>

```

# VALIDATING GENERATED OUTPUT

With --norm-multiallelic every generated output must validate cleanly -
even where the default mode emits MULTI=ALTPROBLEM records:

```

>>> sh("for f in ../test/tmp/vcfputtogetheragain-norm_*.vcf; do vcfvalidate $f 2>&1 >/dev/null | tail -1; done")
vcfvalidate: checked 2 records, found 0 error(s)
vcfvalidate: checked 423 records, found 0 error(s)
vcfvalidate: checked 3 records, found 0 error(s)

```

# LICENSE

Copyright 2025 (C) Pjotr Prins and vcflib contributors. MIT licensed.
