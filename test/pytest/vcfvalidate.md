% VCFVALIDATE(1) vcfvalidate (vcflib) | vcfvalidate (VCF diagnostic)
% Pjotr Prins and vcflib contributors

# NAME

vcfvalidate - validate VCF data lines against the VCFv4.5 specification

# SYNOPSIS

**vcfvalidate** [-q] [file]

# DESCRIPTION

**vcfvalidate** checks every data line of a VCF file against the
field rules of the VCFv4.5 specification (see
[test/data/spec/VCFv4.5.tex](../data/spec/VCFv4.5.tex)) using the same
validator as [vcfputtogetheragain](./vcfputtogetheragain.md) (the
vcf-std library). Errors are reported with the offending line and cite
the section of the specification that is violated. Meta-information
lines are not validated.

Exit status: 0 when all records are valid, 1 when at least one error
was found.

## Options

-h, --help

: shows help message and exits.

-q, --quiet

: suppress the summary line.

See more below.

# EXAMPLES


<!--

    >>> from rtest import run_stdout, head, cat, sh

-->

All records valid:

```

>>> sh("vcfvalidate ../samples/sample.vcf")
vcfvalidate: checked 9 records, found 0 error(s)
<BLANKLINE>

```

A file with three invalid records (duplicate INFO key, empty ALT,
duplicate FILTER codes):

```

>>> sh("vcfvalidate ../test/data/inputs/vcfvalidate-invalid.vcf")
vcfvalidate: line 7: field 7: INFO: duplicate key 'AF' (VCFv4.5 section 1.4.8: duplicate keys not allowed)
  line: chr1	300	.	A	C	29	PASS	AF=0.5;AF=0.5	GT	0|1
vcfvalidate: line 8: field 4: ALT: empty field (use '.' for no variant, VCFv4.5 section 1.4.5)
  line: chr1	400	.	A		29	PASS	.	GT	0|1
vcfvalidate: line 9: field 6: FILTER: duplicate filter code 'q10' (VCFv4.5 section 1.4.7)
  line: chr1	500	.	A	C	29	q10;q10	.	GT	0|1
vcfvalidate: checked 5 records, found 3 error(s)
<BLANKLINE>

```

An empty or header-only file is an error:

```

>>> sh("vcfvalidate ../test/data/inputs/vcfvalidate-empty.vcf")
vcfvalidate: no data records found - file is empty or contains only header lines

```

Quiet mode only sets the exit status:

```

>>> sh("vcfvalidate -q ../test/data/inputs/vcfvalidate-invalid.vcf ; echo exit=$?")
exit=1
<BLANKLINE>

```

# LICENSE

Copyright 2026 (C) Pjotr Prins and vcflib contributors. MIT licensed.
