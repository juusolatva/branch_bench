# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single-file C benchmark (`branch_bench.c`) that measures the real CPU cost of
branch misprediction. It runs four self-contained tests contrasting predictable
vs. unpredictable branching patterns:

- **Test 1 — Threshold Sum**: sorted vs. shuffled array, same direct branch (classic demo).
- **Test 2 — Stride Conditional**: periodic 25% take-rate vs. random 25% take-rate.
- **Test 3 — Indirect Dispatch**: sequential vs. random function-pointer calls (stresses the BTB / indirect predictor).
- **Test 4 — Branch vs. Branchless**: same sum computed via a conditional jump vs. an arithmetic mask, on both sorted and random data, to isolate the misprediction penalty itself.

On Linux, hardware branch/branch-miss counts are read via `perf_event_open(2)`
when the kernel allows it; otherwise everything falls back to wall-clock
timing only (`perf_available()` gates the extra output).

Besides the console output, `-o`/`--output <file>` writes the same results as
a table-formatted Markdown report (see "Editing this code" below for how
each test feeds it).

## Build & run

```sh
./run.sh
```

This is the canonical build+run command — it compiles with the exact flags
the benchmark depends on and then executes the binary:

```sh
gcc -O2 -fno-tree-vectorize -fno-if-conversion -o branch_bench branch_bench.c
./branch_bench
```

Both flags are load-bearing, not just optimization tuning:
- `-fno-tree-vectorize` — prevents SIMD rewriting that would bypass the branches under test.
- `-fno-if-conversion` — keeps `if`/`else` as real conditional jumps instead of the compiler folding them into `cmov`, which is required for Tests 1–3 to actually exercise the branch predictor. Test 4's branchless variant uses an arithmetic mask (`-(u64)(v > T)`) and is unaffected by either flag by construction.

There is no test suite, linter, or other build target in this repo — `run.sh`
is the only workflow. `run.sh` invokes the binary with no arguments (console
output only); pass `-o <file.md>` directly to the binary to also get the
Markdown report.

## Editing this code

- Each test function is `__attribute__((noinline))` — this is required so the
  compiler can't inline the hot loop into `main` and optimize across the
  call boundary in ways that would change what's being measured. Keep new
  benchmark kernels `noinline` too.
- Test 3's 32 leaf functions are generated via the `FOR_EACH_LEAF`/`DEF_LEAF`
  X-macro; each does a distinct arithmetic op specifically so the linker's
  identical-code-folding can't merge them back into one target (which would
  defeat the point of stressing the BTB with distinct call targets). If you
  add leaf functions, keep each one's operation unique.
- Random inputs use a local xorshift64 PRNG (`rng64`/`rng_state`), not
  `stdlib rand()`, to avoid its overhead/bias inside timed regions. Reuse it
  rather than introducing another RNG.
- Decision/index arrays used inside timed loops (e.g. `decisions_random`,
  `dispatch_indices`) are always pre-generated *before* timing starts, so RNG
  cost is never counted as part of the benchmarked branch cost. Preserve this
  separation when adding tests.
- `perf_start`/`perf_stop`/`perf_available` are no-ops on non-Linux or when
  `perf_event_open` is unavailable (`HAVE_PERF` undefined) — don't assume
  `Result.branch_total`/`branch_misses` are populated; always check
  `perf_available() && r.branch_total` before using them, matching the
  existing print blocks.
- `perf_start()` is called just *before* the `now_ms()` timer starts and
  `perf_stop()` just *after* it stops (not nested inside it) — this is
  intentional, so the ioctl/read syscalls aren't counted in `time_ms`. It
  means the perf window is very slightly wider than the timed window; don't
  "fix" this by nesting them back inside the timer, that reintroduces
  syscall overhead into `time_ms` (see `de1beac`/`435dd77`).
- Each `Result.result` needs to stay live across the call boundary or the
  optimizer can eliminate the benchmarked work entirely. Prefer consuming it
  with `anti_dce_sink()` (defined near the `Result` struct) unless it's
  already read by something with an observable side effect — e.g. Test 4's
  branch/branchless sanity-check `fprintf` already forces its four
  `.result` fields live, so it doesn't call `anti_dce_sink()` too.
- Formatted report output: each `run_testN` takes a `FILE *report` (may be
  `NULL` when `-o` wasn't passed) and, alongside its console `printf`s,
  builds a small `ReportRow[]` of `{label, Result}` and calls
  `report_table()` to emit a Markdown table, mirroring the console numbers.
  New tests should follow the same pattern rather than only printing to
  stdout.
