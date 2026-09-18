% VCF-STD(3) vcf-std | VCFv4.5 field validation tests
% Pjotr Prins and vcflib contributors

# NAME

vcf-std - strict VCFv4.5 field validation (plain C, exposed to Python
through the pyvcfstd module). Each rule below cites the section of the
spec (test/data/spec/VCFv4.5.tex). All validators raise ValueError on
invalid input and return None on valid input.

<!--

    >>> from pyvcfstd import *

-->

# FIXED FIELDS (VCFv4.5 section 1.4)

## CHROM (section 1.4.1) - no whitespace

```
>>> check_chrom("scaffold612")
>>> check_chrom("grch38#chr4")
>>> check_chrom("<contig1>")
>>> check_chrom("chr 1")
Traceback (most recent call last):
...
ValueError: CHROM: whitespace not permitted (VCFv4.5 section 1.4.1)
>>> check_chrom("")
Traceback (most recent call last):
...
ValueError: CHROM: empty field (required, VCFv4.5 section 1.4.1)

```

## POS (section 1.4.2) - integer, 0 allowed for telomeres

```
>>> check_pos("1235237")
>>> check_pos("0")
>>> check_pos("1e3")
Traceback (most recent call last):
...
ValueError: POS: 'e' is not a digit - POS must be a non-negative integer (VCFv4.5 section 1.4.2)
>>> check_pos("-5")
Traceback (most recent call last):
...
ValueError: POS: '-' is not a digit - POS must be a non-negative integer (VCFv4.5 section 1.4.2)

```

## ID (section 1.4.3) - semicolon separated unique identifiers

```
>>> check_id("rs6054257")
>>> check_id("rs6054257;rs123")
>>> check_id(".")
>>> check_id("a b")
Traceback (most recent call last):
...
ValueError: ID: whitespace not permitted in identifier 'a b' (VCFv4.5 section 1.4.3)
>>> check_id("rs1;rs1")
Traceback (most recent call last):
...
ValueError: ID: duplicate identifier 'rs1' (VCFv4.5 section 1.4.3: duplicate values not allowed)
>>> check_id("a;;b")
Traceback (most recent call last):
...
ValueError: ID: empty identifier (VCFv4.5 section 1.4.3)

```

## REF (section 1.4.4) - bases A,C,G,T,N, case insensitive

```
>>> check_ref("ACCCCCACCCCCACC")
>>> check_ref("acgtn")
>>> check_ref("R")
Traceback (most recent call last):
...
ValueError: REF: invalid base 'R' at offset 0 - each base must be one of A,C,G,T,N (case insensitive, VCFv4.5 section 1.4.4)
>>> check_ref("<DEL>")
Traceback (most recent call last):
...
ValueError: REF: invalid base '<' at offset 0 - each base must be one of A,C,G,T,N (case insensitive, VCFv4.5 section 1.4.4)

```

## ALT (section 1.4.5) - bases, '.', '*', symbolic and breakends

```
>>> check_alt("C")
>>> check_alt("ACC,AC")
>>> check_alt("*")
>>> check_alt(".")
>>> check_alt("C,*,<DEL>,<NON_REF>")
>>> check_alt("T,.,*")
>>> check_alt("HGVA")
Traceback (most recent call last):
...
ValueError: ALT: invalid base 'H' at offset 0 - each allele must be bases (A,C,G,T,N), '.', '*', symbolic <ID> or a breakend (VCFv4.5 section 1.4.5)
>>> check_alt("<DEL")
Traceback (most recent call last):
...
ValueError: ALT: symbolic allele '<DEL' must be of the form <ID> (VCFv4.5 section 1.4.5)
>>> check_alt("<DE L>")
Traceback (most recent call last):
...
ValueError: ALT: whitespace, comma or angle bracket not permitted inside symbolic allele ID '<DE L>' (VCFv4.5 section 1.4.5)
>>> check_alt("A,,C")
Traceback (most recent call last):
...
ValueError: ALT: empty allele (use '.' for no variant, VCFv4.5 section 1.4.5)

```

Breakend notation (section 5.4):

```
>>> check_alt("A]17:123456]")
>>> check_alt("[13:123457[C")
>>> check_alt("A]chr1:1]",)

```

## QUAL (section 1.4.6 and Float type, section 1.3)

```
>>> check_qual("60")
>>> check_qual("9.6")
>>> check_qual(".")
>>> check_qual("1e-3")
>>> check_qual("-10.5")
>>> check_qual("INF")
>>> check_qual("nan")
>>> check_qual("1.")
Traceback (most recent call last):
...
ValueError: QUAL: '1.' is not a valid VCF Float or '.' (VCFv4.5 sections 1.3 and 1.4.6)
>>> check_qual("60,5")
Traceback (most recent call last):
...
ValueError: QUAL: '60,5' is not a valid VCF Float or '.' (VCFv4.5 sections 1.3 and 1.4.6)

```

## FILTER (section 1.4.7)

```
>>> check_filter("PASS")
>>> check_filter(".")
>>> check_filter("q10;s50")
>>> check_filter("0")
Traceback (most recent call last):
...
ValueError: FILTER: '0' is reserved and must not be used (VCFv4.5 section 1.4.7)
>>> check_filter("PASS;q10")
Traceback (most recent call last):
...
ValueError: FILTER: PASS must be used alone, not in a list (VCFv4.5 section 1.4.7)
>>> check_filter("q10;q10")
Traceback (most recent call last):
...
ValueError: FILTER: duplicate filter code 'q10' (VCFv4.5 section 1.4.7)

```

## INFO (section 1.4.8)

Keys must match ^([A-Za-z_][0-9A-Za-z_.]*|1000G)$, duplicates are not
allowed, flags carry no '=':

```
>>> check_info("DP=154;MQ=52;H2")
>>> check_info(".")
>>> check_info("1000G")
>>> check_info("AC=1,2;AF=0.5,0.25")
>>> check_info("AF=0.5;AF=0.25")
Traceback (most recent call last):
...
ValueError: INFO: duplicate key 'AF' (VCFv4.5 section 1.4.8: duplicate keys not allowed)
>>> check_info("3G=1")
Traceback (most recent call last):
...
ValueError: INFO: invalid key '3G' - keys must match ^([A-Za-z_][0-9A-Za-z_.]*|1000G)$ (VCFv4.5 section 1.4.8)
>>> check_info("DP=")
Traceback (most recent call last):
...
ValueError: INFO: key 'DP' has an empty value (format is key=value, VCFv4.5 section 1.4.8)
>>> check_info("DP=15=4")
Traceback (most recent call last):
...
ValueError: INFO: literal '=' inside value - percent-encode as %3D (VCFv4.5 section 1.5)
>>> check_info("NOTE=hello world")   # spaces allowed in values
>>> check_info("NOTE=50%25")         # valid percent encoding
>>> check_info("NOTE=50%")
Traceback (most recent call last):
...
ValueError: INFO value: bare '%' - percent encoding expects two hex digits (e.g. %3A for ':'), see VCFv4.5 section 1.5

```

# GENOTYPE FIELDS (VCFv4.5 section 1.6.1)

## FORMAT keys

```
>>> check_format("GT:DS:GP")
>>> check_format("GT:HQ")
>>> check_format(".")
>>> check_format("DS:GT")    # GT must come first
Traceback (most recent call last):
...
ValueError: FORMAT: GT must be the first key when present (VCFv4.5 section 1.6.1)
>>> check_format("GT:DP:DP")
Traceback (most recent call last):
...
ValueError: FORMAT: duplicate key 'DP' (VCFv4.5 section 1.6.1)
>>> check_format("GT:1X")
Traceback (most recent call last):
...
ValueError: FORMAT: key '1X' must match ^[A-Za-z_][0-9A-Za-z_.]*$ (VCFv4.5 section 1.6.1)

```

## Sample columns

```
>>> check_sample("GT:HQ", "0|0:10,10")
>>> check_sample("GT:HQ", "0|0:.")        # missing value
>>> check_sample("GT:HQ", "0|0")          # trailing fields may be dropped
>>> check_sample("GT:HQ", ":10,10")       # ...but never the GT field
Traceback (most recent call last):
...
ValueError: sample column is empty but FORMAT declares GT - the GT field must always be present (VCFv4.5 section 1.6.1: only trailing fields can be dropped)
>>> check_sample("GT:HQ", "0|0:10,10:7")  # more values than keys
Traceback (most recent call last):
...
ValueError: sample has 3 value(s) but FORMAT declares 2 key(s) (VCFv4.5 section 1.6.1)
>>> check_sample("GT:HQ", "0|0:")
Traceback (most recent call last):
...
ValueError: sample value 2 is empty - use '.' for missing values (VCFv4.5 section 1.6.1)

```

## GT grammar

```
>>> check_gt("0|1", 1)
>>> check_gt("0/1/2", 2)
>>> check_gt(".", 2)
>>> check_gt("0|.", 2)
>>> check_gt("2|0", 1)
Traceback (most recent call last):
...
ValueError: GT: allele index 2 out of range - only 1 ALT allele(s) declared (indices 0..1 valid, VCFv4.5 section 1.6.1)
>>> check_gt("0/1", 1)      # valid unphased
>>> check_gt("0/1|2", 2)
Traceback (most recent call last):
...
ValueError: GT: mixed phased and unphased separators ('/' and '|') not allowed (VCFv4.5 section 1.6.1)
>>> check_gt("0|", 1)
Traceback (most recent call last):
...
ValueError: GT: trailing separator - expected an allele after '|' (VCFv4.5 section 1.6.1)

```

# FULL RECORDS

```
>>> validate_record("20\t14370\trs6054257\tG\tA\t29\tPASS\tAF=0.5\tGT\t0|0\t1|0\t1/1")
>>> validate_record("20\t17330\t.\tT\tC\t3\tq10\tDP=13\tGT:GQ:DP\t0|0:48:1\t0|1:48:8\t1/1:43:5")
>>> validate_record("20\t111\t.\tA\t.\t9.6\t.\t.\tGT:HQ\t0|0:10,10\t0|0:10,10\t0|0:3,3")
>>> validate_record("chr1\t100\t.\tAC\tA,*,<INS>\t50\tPASS\tAC=1,1,1\tGT:DP\t1/2:5\t0|3:7\t.:2")
>>> validate_record("20\t14370\trs6054257\tG\tA\t29\tPASS\t.\tGT")
Traceback (most recent call last):
...
ValueError: FORMAT column present but no sample columns follow (VCFv4.5 section 1.6.1)
>>> validate_record("20\t14370\t.\tG\tA\t29\tPASS\t.\tGT\t0|0:5")
Traceback (most recent call last):
...
ValueError: sample has more value(s) than the 1 FORMAT key(s) declare (VCFv4.5 section 1.6.1)
>>> validate_record("20\t111\t.\tA\t.\t9.6\t.\t.\tGT:HQ\t0|0:10,10\t0|0:10,10\t0/1:3,3")
Traceback (most recent call last):
...
ValueError: GT: allele index 1 out of range - only 0 ALT allele(s) declared (indices 0..0 valid, VCFv4.5 section 1.6.1)
>>> validate_record("20\t1e3\t.\tG\tA\t.\t.\t.")
Traceback (most recent call last):
...
ValueError: POS: 'e' is not a digit - POS must be a non-negative integer (VCFv4.5 section 1.4.2)
>>> validate_record("20\t100\t.\tG\tG,G\t.\t.\t.\tGT\t3|3")
Traceback (most recent call last):
...
ValueError: GT: allele index 3 out of range - only 2 ALT allele(s) declared (indices 0..2 valid, VCFv4.5 section 1.6.1)

```

# ALT COUNT HELPER

```
>>> count_alt("A,T,C")
3
>>> count_alt(".")
0
>>> count_alt("")
Traceback (most recent call last):
...
ValueError: ALT: malformed field

```

# LICENSE

Copyright 2026 (C) Pjotr Prins and vcflib contributors. MIT licensed.
