# AGENTS.md

This file provides guidance and technical context for AI agents working on code in this repository.

## Project Overview

`branch_bench` is a single-file C microbenchmark ([`branch_bench.c`](branch_bench.c)) designed to measure the real CPU performance penalties associated with branch mispredictions across modern processor pipelines.

It runs four self-contained tests comparing predictable versus unpredictable branching patterns:

1. **Test 1 — Threshold Sum**: Sorted vs. shuffled array filtering elements above a threshold (`128`). Measures direct branch prediction behavior.
2. **Test 2 — Stride Conditional**: Periodic 25% take-rate ("every 4th") vs. random 25% take-rate. Evaluates pattern history table learning.
3. **Test 3 — Indirect Dispatch**: Sequential vs. random function pointer dispatch across 32 distinct leaf functions. Evaluates the history-based indirect branch predictor (a plain Branch Target Buffer alone cannot learn the 32-target cycle).
4. **Test 4 — Branch vs. Branchless**: Computes identical threshold sums using conditional jumps vs. arithmetic bitmasks (`(u64)0 - (u64)(arr[i] > THRESHOLD)`), across both sorted and random datasets to isolate the pure misprediction cycle penalty.

On Linux systems supporting `perf_event_open(2)`, the benchmark reads hardware performance counters (`PERF_COUNT_HW_BRANCH_INSTRUCTIONS` and `PERF_COUNT_HW_BRANCH_MISSES`). When hardware counters are unavailable (e.g. non-Linux, locked down VMs/containers), it gracefully falls back to monotonic wall-clock timing (`now_ms()`). The wall-clock fallback uses POSIX `clock_gettime(CLOCK_MONOTONIC)`, so the benchmark needs a POSIX system; it does not build with MSVC.

---

## Build, Run, and Verification

### Canonical Run Command
```sh
./run.sh
```

The script compiles the code and immediately executes the benchmark:
```sh
gcc -std=c99 -O2 -fno-tree-vectorize -fno-if-conversion -o branch_bench branch_bench.c
./branch_bench
```

### Critical Compiler Flags
Both optimization flags are strictly load-bearing:
- `-fno-tree-vectorize`: Prevents the compiler from vectorizing loops with SIMD instructions, which would eliminate branches altogether.
- `-fno-if-conversion`: Prevents the compiler from converting `if`/`else` control flow into conditional move (`cmov`) instructions. Real branch instructions are required for Tests 1–3 to trigger branch prediction logic.

`-std=c99` enforces the C99 requirement at build time. POSIX and Linux APIs (`clock_gettime`, `syscall`, `localtime_r`) are exposed by the `#define _DEFAULT_SOURCE` at the top of `branch_bench.c`, which must stay before the first `#include`. Do not replace it with `_POSIX_C_SOURCE`: that hides `syscall()` and breaks the build.

### Compiler Support
GCC is the reference compiler. Clang rejects `-fno-if-conversion` and has no equivalent flag; it builds with `clang -std=c99 -O2 -fno-vectorize -fno-slp-vectorize`, but may still emit `cmov` for Tests 1–3, so verify the disassembly (`objdump -d`) before trusting Clang results.

### CLI Options
- `./branch_bench`: Runs all four benchmarks and outputs results to stdout.
- `./branch_bench -o <file.md>` / `./branch_bench --output <file.md>`: Generates a structured Markdown report matching the format used in [`RESULTS.md`](RESULTS.md) alongside the stdout stream.
- `./branch_bench -h` / `./branch_bench --help`: Displays usage information.

### Testing and Validation
There is no dedicated test framework or linter in this repository. Validate changes by compiling with `gcc` using the required flags (plus `-Wall -Wextra -pedantic` to catch issues) and executing the binary. Test 4 includes a built-in runtime sanity check verifying that branch and branchless implementations compute identical sums.

---

## Code Architecture and Implementation Conventions

### 1. Function Inlining Prevention
All benchmark kernels (such as `sum_threshold`, `count_decisions`, `dispatch_sequential`, `dispatch_random`, `sum_branch`, and `sum_branchless`) must be annotated with `__attribute__((noinline))`. This guarantees that the compiler will not inline hot loops into caller functions or optimize across call boundaries.

### 2. Dead Code Elimination (DCE) Prevention
Because benchmark loops compute values without external side effects during timing, the compiler could eliminate the loop entirely.
- Always retain or consume the computed output.
- Use [`anti_dce_sink(u64 v)`](branch_bench.c#L69-L73) on computed results, which forces the value into a `volatile u64` sink.
- If a test performs an observable validation (such as Test 4's result comparison check), an additional `anti_dce_sink` call may be omitted.

### 3. Separation of Setup vs. Timed Execution
- Never allocate memory, perform I/O, or invoke the random number generator inside the timed measurement loop.
- Array generation, shuffling, and table initialization (e.g. `decisions_random`, `dispatch_indices`) must occur *before* starting the timer.
- Free all allocated resources once the benchmark passes complete.

### 4. Deterministic and Low-Overhead PRNG
Use the local xorshift64 PRNG (`rng64()` / `rng_state`) instead of `stdlib rand()`. This avoids glibc locking overhead and distribution skew.

### 5. Indirect Dispatch and Identical Code Folding (ICF)
Test 3 generates 32 leaf functions via X-macros (`FOR_EACH_LEAF` / `DEF_LEAF`). Each leaf function performs a distinct arithmetic operation based on a unique Weyl sequence step (`0x9e3779b97f4a7c15ULL`). This prevents the linker's Identical Code Folding optimization from merging the function pointers into a single destination address.

### 6. Perf Timing Window Convention
- Measure every kernel call with `MEASURE(result, kernel(args))`. It calls `perf_start()` immediately *before* the `now_ms()` timer starts and `perf_stop()` immediately *after* it stops.
- Do **not** nest `perf_start()` or `perf_stop()` inside the `now_ms()` timing window; doing so introduces system call overhead into the wall-clock measurements.
- `MEASURE` is a macro, not a function, on purpose: the kernel must be called directly (not through a function pointer) so its codegen stays unchanged.
- Always guard access to counter fields using `if (perf_available() && r.branch_total)` (`print_row()` and `report_table()` already do).

### 7. Formatted Report Emission
Each test function receives `FILE *report` (which will be `NULL` if `-o` was not specified). Any new test should print each console row with `print_row(label, &result)`, then construct a `ReportRow` array and call `report_table(report, title, desc, rows, count)` to maintain consistent console and Markdown output. Console labels may carry extra padding for alignment; report labels should not.

---

## Git and Contribution Guidelines
- Do not commit changes unless explicitly requested by the user.
- Keep the codebase self-contained within [`branch_bench.c`](branch_bench.c).
- Maintain C99 compatibility (C99 language plus POSIX/Linux APIs, built with GCC) without unnecessary external dependencies. GNU extensions already in use, such as `__attribute__((noinline))`, are accepted.

---

## TODO

### Measurement quality
- **Count CPU cycles:** open `PERF_COUNT_HW_CPU_CYCLES` alongside the branch counters and report cycles per miss directly. README's per-miss figures currently assume each CPU ran at the clock listed in its RESULTS.md heading, which turbo and power-saving make unreliable.
- **Repeat measurements:** every variant currently runs once with no warm-up, so early tests can run before the CPU has reached full clock speed. Run a warm-up pass, then 3–5 trials per variant and report the median (optionally min/max). Keep the perf timing window convention (section 6 above) for every trial.
- **Count branches and misses as one perf group:** open the misses counter (and cycles, once added) with the branches fd as `group_fd`, and read them in one go with `PERF_FORMAT_GROUP`, so all counters cover exactly the same window. Add `PERF_FORMAT_TOTAL_TIME_ENABLED`/`_RUNNING` to detect when the kernel was time-sharing the counters. Related: `perf_available()` is true if *either* counter opened, but rows only show counts when the branches counter works.
- **Verify the ARM raw-event fallback on hardware:** `perf_init()` falls back to raw events `0x12` (`BR_PRED`) and `0x10` (`BR_MIS_PRED`) when the generic events fail. Misses previously used `0x21` (`BR_RETIRED`, which counts all retired branches); the fix has only been compile-checked. Test it on a Raspberry Pi 4 and a Cudy WR3000S (Cortex-A53, OpenWrt). On OpenWrt, check that the kernel has `CONFIG_PERF_EVENTS` enabled, and expect "Unknown CPU" because arm64 `/proc/cpuinfo` has no model line there.

### Reporting and usability
- **Clock speed in the report heading:** read `/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq` (when present) and append it to the CPU heading, e.g. `@ 3.60 GHz`. It is currently added to RESULTS.md by hand.
- **CLI options:** `-t <n>` to run a single test, and `--size`/`--reps` to override `ARRAY_LEN`/`REPS` (useful for small boards and quick CI runs). Report non-default values in the output so results stay comparable.
- **Misses-per-element column:** the Miss % column divides by all branches, including the loop's back-edge, so Test 1's ~50% data-branch miss rate shows as ~25%. A misses-per-element (or per-call for Test 3) column would show the data branch's real miss rate directly.

---

## Long-Term Goals

### Portability to more architectures (MIPS, RISC-V, PowerPC, ...)
The benchmark is developed and tested on x86-64 and ARM (Cortex-A72) Linux. Supporting other architectures is a long-term goal; known gaps:
- **CPU model detection:** `get_cpu_model()` only recognizes the x86 `model name` and ARM `Model`/`Hardware` keys in `/proc/cpuinfo`. MIPS uses `cpu model` (plus `system type`), PowerPC uses `cpu`, and RISC-V often has only `isa`/`uarch`, so these currently report "Unknown CPU".
- **Perf counters:** generic `PERF_COUNT_HW_BRANCH_*` events are tried first everywhere, but raw-event fallbacks exist only for ARM. Many embedded MIPS cores have no PMU or no kernel PMU driver, so these fall back to wall-clock timing only.
- **Branch codegen:** `-fno-if-conversion` must still leave real branches in Tests 1–3. Check the disassembly on each new architecture: MIPS has conditional moves (`movn`/`movz`, `seleqz`/`selnez` on R6) and RISC-V has `czero` (Zicond).
- **32-bit targets:** the `u64` accumulators and Test 3 leaf arithmetic become multi-instruction sequences on 32-bit cores (e.g. MIPS32), which changes the per-iteration work and makes timings incomparable with 64-bit results.
- **Memory footprint:** the tests allocate several 4 MiB buffers; small embedded boards may need `ARRAY_LEN`/`DISPATCH_N` to be configurable.
- **Interpretation:** README's interpretation section is written for deep out-of-order cores. In-order cores (common on MIPS) have much shorter misprediction penalties.
- **Validation:** cross-compile (e.g. `mips-linux-gnu-gcc`) and run under `qemu-user` to check correctness and output format. Timings under emulation are meaningless; real hardware is required for results.

### GitHub Actions CI
Add a workflow that runs on every push and pull request:
- Build with `gcc -std=c99 -Wall -Wextra -pedantic -Werror` using the required flags, and with Clang (see Compiler Support).
- Run the binary with a reduced size (requires the `--size`/`--reps` TODO) so Test 4's sanity check executes; that check must fail the run (non-zero exit) on a mismatch, not just print a warning.
- Check the disassembly of the Test 1–3 kernels for `cmov`. Whether real branches survive depends on the compiler version, and losing them silently invalidates results.
- Optionally run the SonarQube Cloud analysis from the same workflow.

Hosted runners are VMs, usually without hardware perf counters, so CI checks correctness and codegen only, not timings.
