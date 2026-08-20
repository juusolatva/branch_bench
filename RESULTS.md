## Intel Core i5-3380M @ 3.60 GHz

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

## Intel Core i7-4770 @ 3.90 GHz

══════════════════════════════════════════════════════════════
  CPU Branch Prediction Benchmark
  Array size:         4194304 elements
  Repetitions:             64 per trial
  Perf counters:   AVAILABLE (hardware branch-miss counts shown)
══════════════════════════════════════════════════════════════

TEST 1 — Threshold Sum  (sorted vs shuffled array, 4194304×64 reps)
  Counts/sums elements > 128.  Identical data, different order.

  Sorted   (predictable, ~0% misses)        119.0 ms  branches  536871132  misses       308  (0.0%)
  Shuffled (unpredictable, ~50% misses)     847.8 ms  branches  536871862  misses 134233487  (25.0%)
  → Slowdown:  7.12×

TEST 2 — Stride Conditional  (periodic vs random 25%, 4194304×64 reps)
  Both arrays have ~25% ones; only the pattern differs.

  Periodic (every 4th — learnable)        121.3 ms  branches  536871120  misses       260  (0.0%)
  Random   (same rate — unlearnable)      611.8 ms  branches  536871620  misses  77057133  (14.4%)
  → Slowdown:  5.04×

TEST 3 — Indirect Dispatch  (32 targets, 4M calls)
  Calls leaf functions via pointer. BTB must predict the target address.

  Sequential i%32 (BTB learns cycle)          5.4 ms  branches   12582941  misses      4214  (0.0%)
  Random index (BTB always wrong)            28.2 ms  branches   12582965  misses   4062684  (32.3%)
  → Slowdown:  5.19×

TEST 4 — Branch vs Branchless  (sorted and random data, 4194304×64 reps)
  Branchless uses arithmetic mask: -(u64)(v > T) to avoid jumps.

  Sorted  + branch                          119.0 ms  branches  536871119  misses       273  (0.0%)
  Sorted  + branchless                      167.2 ms  branches  268435711  misses        82  (0.0%)
  Random  + branch                          847.5 ms  branches  536871860  misses 134204136  (25.0%)
  Random  + branchless                      167.3 ms  branches  268435713  misses        73  (0.0%)

  Branch penalty on random data:  7.12× vs sorted-branch
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

## AMD Ryzen 5 9600X @ ~5.50 GHz (WSL2)

══════════════════════════════════════════════════════════════
  CPU Branch Prediction Benchmark
  Array size:         4194304 elements
  Repetitions:             64 per trial
  Perf counters:   AVAILABLE (hardware branch-miss counts shown)
══════════════════════════════════════════════════════════════

TEST 1 — Threshold Sum  (sorted vs shuffled array, 4194304×64 reps)
  Counts/sums elements > 128.  Identical data, different order.

  Sorted   (predictable, ~0% misses)         49.8 ms  branches  536871052  misses      1270  (0.0%)
  Shuffled (unpredictable, ~50% misses)     659.3 ms  branches  536871682  misses 134135951  (25.0%)
  → Slowdown:  13.23×

TEST 2 — Stride Conditional  (periodic vs random 25%, 4194304×64 reps)
  Both arrays have ~25% ones; only the pattern differs.

  Periodic (every 4th — learnable)         52.4 ms  branches  536871055  misses      4911  (0.0%)
  Random   (same rate — unlearnable)      387.2 ms  branches  536871395  misses  71383292  (13.3%)
  → Slowdown:  7.39×

TEST 3 — Indirect Dispatch  (32 targets, 4M calls)
  Calls leaf functions via pointer. BTB must predict the target address.

  Sequential i%32 (BTB learns cycle)          4.6 ms  branches   12582939  misses       179  (0.0%)
  Random index (BTB always wrong)            28.5 ms  branches   12582962  misses   4063014  (32.3%)
  → Slowdown:  6.16×

TEST 4 — Branch vs Branchless  (sorted and random data, 4194304×64 reps)
  Branchless uses arithmetic mask: -(u64)(v > T) to avoid jumps.

  Sorted  + branch                           49.7 ms  branches  536871047  misses      1270  (0.0%)
  Sorted  + branchless                       66.5 ms  branches  268435611  misses       644  (0.0%)
  Random  + branch                          657.2 ms  branches  536871675  misses 134180972  (25.0%)
  Random  + branchless                       65.8 ms  branches  268435610  misses       622  (0.0%)

  Branch penalty on random data:  13.21× vs sorted-branch
  Branchless is consistent:       0.99× random vs sorted

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

## ARM Cortex-A72 @ 1.60 GHz (Broadcom BCM2711)

══════════════════════════════════════════════════════════════
  CPU Branch Prediction Benchmark
  Array size:         4194304 elements
  Repetitions:             64 per trial
  Perf counters:   AVAILABLE (hardware branch-miss counts shown)
══════════════════════════════════════════════════════════════

TEST 1 — Threshold Sum  (sorted vs shuffled array, 4194304×64 reps)
  Counts/sums elements > 128.  Identical data, different order.

  Sorted   (predictable, ~0% misses)        537.3 ms  branches  536872495  misses       466  (0.0%)
  Shuffled (unpredictable, ~50% misses)    2232.6 ms  branches  634490117  misses 134093379  (21.1%)
  → Slowdown:  4.15×

TEST 2 — Stride Conditional  (periodic vs random 25%, 4194304×64 reps)
  Both arrays have ~25% ones; only the pattern differs.

  Periodic (every 4th — learnable)        801.8 ms  branches  536872274  misses       335  (0.0%)
  Random   (same rate — unlearnable)     1662.2 ms  branches  597816572  misses  85074284  (14.2%)
  → Slowdown:  2.07×

TEST 3 — Indirect Dispatch  (32 targets, 4M calls)
  Calls leaf functions via pointer. BTB must predict the target address.

  Sequential i%32 (BTB learns cycle)         61.1 ms  branches   12583075  misses   3670044  (29.2%)
  Random index (BTB always wrong)            75.3 ms  branches   16701752  misses   4063036  (24.3%)
  → Slowdown:  1.23×

TEST 4 — Branch vs Branchless  (sorted and random data, 4194304×64 reps)
  Branchless uses arithmetic mask: -(u64)(v > T) to avoid jumps.

  Sorted  + branch                          536.2 ms  branches  536872840  misses       476  (0.0%)
  Sorted  + branchless                      554.9 ms  branches  268435890  misses        74  (0.0%)
  Random  + branch                         2231.9 ms  branches  634601595  misses 134229021  (21.2%)
  Random  + branchless                      554.8 ms  branches  268435901  misses        71  (0.0%)

  Branch penalty on random data:  4.16× vs sorted-branch
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

## ARM Cortex-A53 @ 1.30 GHz (Filogic 820)

══════════════════════════════════════════════════════════════
  CPU Branch Prediction Benchmark
  Array size:         4194304 elements
  Repetitions:             64 per trial
  Perf counters:   unavailable — showing wall-clock timing only
══════════════════════════════════════════════════════════════

TEST 1 — Threshold Sum  (sorted vs shuffled array, 4194304×64 reps)
  Counts/sums elements > 128.  Identical data, different order.

  Sorted   (predictable, ~0% misses)       1023.1 ms
  Shuffled (unpredictable, ~50% misses)    1840.0 ms
  → Slowdown:  1.80×

TEST 2 — Stride Conditional  (periodic vs random 25%, 4194304×64 reps)
  Both arrays have ~25% ones; only the pattern differs.

  Periodic (every 4th — learnable)       1043.9 ms
  Random   (same rate — unlearnable)     1538.3 ms
  → Slowdown:  1.47×

TEST 3 — Indirect Dispatch  (32 targets, 4M calls)
  Calls leaf functions via pointer. BTB must predict the target address.

  Sequential i%32 (BTB learns cycle)         60.4 ms
  Random index (BTB always wrong)            74.3 ms
  → Slowdown:  1.23×

TEST 4 — Branch vs Branchless  (sorted and random data, 4194304×64 reps)
  Branchless uses arithmetic mask: -(u64)(v > T) to avoid jumps.

  Sorted  + branch                         1019.4 ms
  Sorted  + branchless                     1069.3 ms
  Random  + branch                         1846.3 ms
  Random  + branchless                     1065.7 ms

  Branch penalty on random data:  1.81× vs sorted-branch
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
