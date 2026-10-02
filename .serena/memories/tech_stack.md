# Tech stack

- C99, one translation unit. Headers: stdio/stdlib/stdint/string/time/inttypes; on Linux also unistd, sys/syscall, linux/perf_event, sys/ioctl.
- Compilers: gcc (canonical, used by `run.sh`) or clang. There's no Makefile/CMake and no package manager.
- Required flags (load-bearing, never drop them): `-O2 -fno-tree-vectorize -fno-if-conversion`. Without them the compiler vectorizes or cmov-converts the kernels and the mispredictions disappear. Test 4's branchless variant doesn't depend on the flags.
- Perf counters: `perf_event_open(2)` in user mode (`exclude_kernel`, `exclude_hv`). They may be unavailable when `kernel.perf_event_paranoid` is restrictive or inside containers/WSL; the program then falls back to wall-clock timing via `now_ms()` (monotonic clock).
- Platforms with results so far: Intel x86 laptops/desktops, AMD Zen 5 under WSL2, Raspberry Pi 4 (AArch64).
- The dev machine is Fedora Linux, x86_64.
