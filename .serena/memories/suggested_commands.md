# Suggested commands (run from repo root)

- Build and run: `./run.sh`
- Build with warnings: `gcc -std=c99 -O2 -fno-tree-vectorize -fno-if-conversion -Wall -Wextra -pedantic -o branch_bench branch_bench.c`
- Clang build check: `clang -std=c99 -O2 -fno-vectorize -fno-slp-vectorize -Wall -Wextra -pedantic -o /tmp/bb branch_bench.c` (clang rejects `-fno-if-conversion`; its timings aren't trusted)
- Static analysis (mirrors SonarQube Cloud's path-sensitive C checks): `clang --analyze -std=c99 branch_bench.c` — must print no warnings (writes a `.plist` in the cwd; run from a scratch dir)
- Markdown report: `./branch_bench -o report.md`; steadier numbers: `-n 5`; quick/small runs: `-t <N>`, `--size 64K --reps 2`. Help: `./branch_bench --help`. To add a machine to `RESULTS.md`, append the report's `## <CPU>` section by hand (defaults only).
- Kernel disassembly: `objdump -d --no-show-raw-insn branch_bench | awk '/<sum_branch>:/,/^$/'`. The body shared by `sum_threshold` and `sum_branch` is listed only under `sum_branch`; other kernels may carry suffixes like `.constprop.0`.
- Perf counter permission: `cat /proc/sys/kernel/perf_event_paranoid` (≤ 2 needed).
- SonarQube Cloud issues (public project, no auth): `curl -s "https://sonarcloud.io/api/issues/search?componentKeys=juusolatva_branch_bench&resolved=false&ps=100"`
- Full default run takes ~10 s (4M elements × 64 reps per variant); `-n N` takes about N+1 times as long.
