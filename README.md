# branch bench

A simple benchmark for testing out gains from branch prediction or conversely losses from mispredictions.

## Notes

You **MUST** run this with ./run.sh or use the flags *-fno-tree-vectorize* *-fno-if-conversion* when compiling or the compiler will optimize the mispredictions away.

## Saving results

Console output is meant to be read as it scrolls by. To get a clean,
table-formatted Markdown report instead use `-o`/`--output`:

```sh
./branch_bench -o report.md
```

This writes the report file in addition to the normal
console output.

The report heading shows the CPU's maximum (turbo/boost) clock where
Linux reports it. The CPU rarely holds that clock under sustained load,
so the report also ends with the **average clock during measurements**,
calculated from the cycle counter. That is the clock the results were
actually produced at, and it often explains why timings differ between
runs on the same machine.

## Repeated measurements

By default each variant is measured once. For steadier numbers use
`-n`/`--trials`:

```sh
./branch_bench -n 5 -o report.md
```

Each variant then gets one untimed warm-up run followed by 5 measured
trials. The trial with the median time is reported, together with the
fastest–slowest range, so you can see how noisy the run was. Expect the
run to take about N+1 times as long.

## Interpreting the results

A branch misprediction costs a pipeline flush: the work fetched down the
wrong path is thrown away and the front end restarts at the correct
target. The minimum penalty is roughly the pipeline depth, typically
~15–20 cycles on modern out-of-order x86 cores.

When the CPU cycle counter is available, each test prints a **Cost per
miss**: the extra cycles divided by the extra mispredictions between its
predictable and unpredictable variant. It is measured in cycles, so it
doesn't depend on the clock speed the CPU happened to run at. On the
machines in [RESULTS.md](RESULTS.md) it is ~20–25 cycles for Test 1.

**Miss %** is misses divided by *all* branches in the timed loop,
including the loop's own back-edge branch, which is almost always
predicted correctly. Test 1's shuffled branch is mispredicted ~50% of the
time, but with two branches per element that shows up as ~25%.

**Sanity check for Test 1:** each repetition walks 4,194,304 elements. At
a 50% miss rate that is ~2.1M misses per repetition; at ~20 cycles each
that is ~42M cycles, or ~10 ms per repetition at 4 GHz. That matches
the measured shuffled-minus-sorted difference of ~9–13 ms per repetition
on the x86 machines in RESULTS.md.

- **Tests 1 and 2** use direct conditional branches (a jump inside the
  loop). Test 2 shows the predictor learning a repeating pattern (every
  4th element taken) but not a random one with the same 25% rate.
- **Test 3** uses indirect calls through a function pointer, so the CPU
  must predict a target address, not just taken/not-taken. A plain branch
  target buffer (BTB) remembers only the last target of each branch and
  would miss on every call of the sequential `i % 32` cycle; learning that
  cycle takes an indirect predictor that uses branch history. Haswell
  (i7-4770) and newer cores in RESULTS.md predict it almost perfectly,
  while the Ivy Bridge i5-3380M and the Cortex-A72 still miss ~28 of every
  32 calls. With random indices every core is right only by chance, about
  1 call in 32. On the Intel machines a missed indirect call costs about
  the same as a missed conditional branch (~22–24 cycles).
- **Test 4** removes the data-dependent branch but not the loop branch,
  which is why its branch count halves. Its time barely changes between
  sorted and random data, but it is not automatically faster: on sorted
  data the branching version wins on most machines in RESULTS.md (e.g.
  119 ms vs 167 ms on the i7-4770), because a correctly predicted branch
  is nearly free while the branchless version always does the extra work.
