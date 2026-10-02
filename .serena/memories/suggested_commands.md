# Suggested commands (run from repo root)

- Build and run: `./run.sh`
- Build only: `gcc -O2 -fno-tree-vectorize -fno-if-conversion -o branch_bench branch_bench.c` (swap in `clang` to check portability)
- Markdown report: `./branch_bench -o report.md` (also prints the normal console output). To add a machine to `RESULTS.md`, append the report's `## <CPU>` section by hand.
- Help: `./branch_bench --help`
- Confirm branches survived compilation: `objdump -d branch_bench | less` and look in kernels such as `sum_threshold` / `count_decisions` for conditional jumps rather than cmov or SIMD.
- Check whether perf counters are allowed: `cat /proc/sys/kernel/perf_event_paranoid` (≤2 is needed for user-space HW counters).
- A full run takes several seconds (4M elements × 64 reps per variant).
