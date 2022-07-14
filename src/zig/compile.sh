#!/bin/sh

zig build test
zig build
exit $?

# C example:
zig cc -o hello test_zig.c zig-out/lib/libzig.a
./hello
