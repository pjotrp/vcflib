#!/bin/bash

zig build test
zig build
zig cc -o hello test_zig.c zig-out/lib/libzig.a
./hello
