# Conventions

Full rules are in `AGENTS.md` (sections 1–7). Key points:

- Kernels are `static __attribute__((noinline))` and return a value that gets consumed: either through `anti_dce_sink(u64)` (writes to a volatile sink) or through an observable check (Test 4 compares the branch and branchless sums).
- Timing window per variant: `perf_start(); t = now_ms(); <kernel loop>; rs.time_ms = now_ms() - t; perf_stop();`. Nothing goes inside it except the kernel: no allocation, RNG, I/O or syscalls.
- Read counters only under `if (perf_available() && r.branch_total)`.
- Randomness comes only from the xorshift64 `rng64()` / `rng_state`, never `rand()`.
- Test 3 leaves are generated with `FOR_EACH_LEAF(DEF_LEAF)` / `PTR_LEAF`. Each leaf must do distinct arithmetic so linker ICF can't merge them. To add leaves, extend the X-macro and keep `NUM_FUNCS` in sync.
- Results are stored as `Result` structs (`time_ms`, branch counters). For the report, a test builds `ReportRow rows[]` and calls `report_table(report, title, desc, rows, n)`, then prints a `**Slowdown:** X×` line. The console prints `  → Slowdown:  X×`.
- Types: `u64`/`u8` typedefs. Format u64 with `PRIu64`.
- Style: 4-space indent, K&R braces, a section-banner comment before each block (`/* ── Title ─── */`), aligned `#define` comments, `static` on every file-scope symbol.
- Setup (allocation, shuffle, table init) happens before timing, and everything is freed at the end of each `run_testN`.
