## Intel(R) Core(TM) i5-3380M CPU @ 2.90GHz

══════════════════════════════════════════════════════════════
  CPU Branch Prediction Benchmark
  Array size:         4194304 elements
  Repetitions:             64 per trial
  Perf counters:   AVAILABLE (hardware branch-miss counts shown)
══════════════════════════════════════════════════════════════

TEST 1 — Threshold Sum  (sorted vs shuffled array, 4194304×64 reps)
  Counts/sums elements > 128.  Identical data, different order.

  Sorted   (predictable, ~0% misses)        155.0 ms  branches  536871160  misses      1019  (0.0%)
  Shuffled (unpredictable, ~50% misses)     938.4 ms  branches  536871966  misses 134129636  (25.0%)
  → Slowdown:  6.06×

TEST 2 — Stride Conditional  (periodic vs random 25%, 4194304×64 reps)
  Both arrays have ~25% ones; only the pattern differs.

  Periodic (every 4th — learnable)        205.3 ms  branches  536871207  misses      9357  (0.0%)
  Random   (same rate — unlearnable)      666.4 ms  branches  536871682  misses  70140973  (13.1%)
  → Slowdown:  3.25×

TEST 3 — Indirect Dispatch  (32 targets, 4M calls)
  Calls leaf functions via pointer. BTB must predict the target address.

  Sequential i%32 (BTB learns cycle)         29.1 ms  branches   12582967  misses   3931969  (31.2%)
  Random index (BTB always wrong)            33.3 ms  branches   12582970  misses   4063264  (32.3%)
  → Slowdown:  1.15×

TEST 4 — Branch vs Branchless  (sorted and random data, 4194304×64 reps)
  Branchless uses arithmetic mask: -(u64)(v > T) to avoid jumps.

  Sorted  + branch                          154.5 ms  branches  536871155  misses       931  (0.0%)
  Sorted  + branchless                      210.8 ms  branches  268435758  misses        90  (0.0%)
  Random  + branch                          937.9 ms  branches  536871950  misses 134191148  (25.0%)
  Random  + branchless                      211.1 ms  branches  268435758  misses        91  (0.0%)

  Branch penalty on random data:  6.07× vs sorted-branch
  Branchless is consistent:       1.00× random vs sorted

══════════════════════════════════════════════════════════════
  Interpretation guide
  ─────────────────────────────────────────────────────────
  Misprediction penalty = pipeline flush + refill latency.
  On typical x86 out-of-order CPUs this is ~15–20 cycles.
  At 50% miss rate on 1M branches/rep: ~8M wasted cycles/rep.
  At 4 GHz that is ~2 ms overhead per rep — matches Test 1.

  Tests 1/2: direct branches (conditional jumps in the loop).
  Test 3:    indirect branches (call via register / BTB);
             penalty per miss is often higher than direct.
  Test 4:    branchless mask trick eliminates branches entirely;
             consistent throughput regardless of data order.
══════════════════════════════════════════════════════════════


