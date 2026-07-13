#!/bin/sh
gcc -O2 -fno-tree-vectorize -fno-if-conversion -o branch_bench branch_bench.c
./branch_bench
