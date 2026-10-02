# Task completion checklist

No test framework, linter or formatter exists. Verify with:

1. `gcc -O2 -fno-tree-vectorize -fno-if-conversion -Wall -Wextra -o branch_bench branch_bench.c` — must build cleanly (ideally also with `clang`).
2. `./branch_bench` — all four tests run; Test 4's built-in sanity check (branch sum == branchless sum) must not report a mismatch. Expect shuffled/random variants to be clearly slower than sorted/periodic on Tests 1, 2 and 4-branch.
3. If report output changed: `./branch_bench -o <scratch>.md` and check that the Markdown matches the `RESULTS.md` format.
4. If a kernel changed: spot-check the disassembly to confirm real conditional branches remain (no cmov or vectorization) and the loop wasn't dead-code-eliminated.
5. Code without `HAVE_PERF` must still compile (non-Linux path): keep perf code inside `#ifdef HAVE_PERF`.

Don't commit unless the user asks.
