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

## Interpreting the results

A branch misprediction costs a pipeline flush: the work fetched down the
wrong path is thrown away and the front end restarts at the correct
target. The minimum penalty is roughly the pipeline depth, typically
~15–20 cycles on modern out-of-order x86 cores. Test 1 on the machines in
[RESULTS.md](RESULTS.md) measures an effective ~20–25 cycles per miss
(assuming each CPU ran at the clock listed in its heading).

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
