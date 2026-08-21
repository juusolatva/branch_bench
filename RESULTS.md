## Intel(R) Core(TM) i5-3380M CPU @ 3.60 GHz

_2026-08-21_

| Array size | Repetitions | Perf counters |
|---:|---:|---|
| 4194304 elements | 64/trial | available (hardware counts) |

### Test 1 — Threshold Sum

Sorted vs shuffled array, same direct branch. Identical data, different order.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sorted (predictable, ~0% misses) | 155.3 | 536871305 | 932 | 0.0% |
| Shuffled (unpredictable, ~50% misses) | 934.3 | 536871987 | 134132567 | 25.0% |

**Slowdown:** 6.02×

### Test 2 — Stride Conditional

Both arrays have ~25% ones; only the pattern differs.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Periodic (every 4th — learnable) | 205.8 | 536871271 | 10329 | 0.0% |
| Random (same rate — unlearnable) | 664.7 | 536871714 | 70296278 | 13.1% |

**Slowdown:** 3.23×

### Test 3 — Indirect Dispatch

32 targets, function-pointer call. BTB must predict the target address.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sequential i%32 (BTB learns cycle) | 28.2 | 12583001 | 3669784 | 29.2% |
| Random index (BTB always wrong) | 33.0 | 12583004 | 4063349 | 32.3% |

**Slowdown:** 1.17×

### Test 4 — Branch vs Branchless

Same sum, computed via conditional jump vs. arithmetic mask, on sorted and random data.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sorted + branch | 154.9 | 536871193 | 927 | 0.0% |
| Sorted + branchless | 207.1 | 268435787 | 84 | 0.0% |
| Random + branch | 934.5 | 536871980 | 134193939 | 25.0% |
| Random + branchless | 206.1 | 268435798 | 80 | 0.0% |

**Branch penalty on random data:** 6.03× vs sorted-branch

**Branchless is consistent:** 0.99× random vs sorted

### Interpretation guide

```
Misprediction penalty = pipeline flush + refill latency.
On typical x86 out-of-order CPUs this is ~15-20 cycles.
At 50% miss rate on 1M branches/rep: ~8M wasted cycles/rep.
At 4 GHz that is ~2 ms overhead per rep — matches Test 1.

Tests 1/2: direct branches (conditional jumps in the loop).
Test 3:    indirect branches (call via register / BTB);
           penalty per miss is often higher than direct.
Test 4:    branchless mask trick eliminates branches entirely;
           consistent throughput regardless of data order.
```

## Intel(R) Core(TM) i7-4770 CPU @ 3.90 GHz

_2026-08-21_

| Array size | Repetitions | Perf counters |
|---:|---:|---|
| 4194304 elements | 64/trial | available (hardware counts) |

### Test 1 — Threshold Sum

Sorted vs shuffled array, same direct branch. Identical data, different order.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sorted (predictable, ~0% misses) | 119.2 | 536871261 | 326 | 0.0% |
| Shuffled (unpredictable, ~50% misses) | 848.7 | 536871891 | 134213194 | 25.0% |

**Slowdown:** 7.12×

### Test 2 — Stride Conditional

Both arrays have ~25% ones; only the pattern differs.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Periodic (every 4th — learnable) | 130.5 | 536871163 | 392 | 0.0% |
| Random (same rate — unlearnable) | 615.3 | 536871653 | 77666566 | 14.5% |

**Slowdown:** 4.71×

### Test 3 — Indirect Dispatch

32 targets, function-pointer call. BTB must predict the target address.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sequential i%32 (BTB learns cycle) | 5.4 | 12582973 | 127 | 0.0% |
| Random index (BTB always wrong) | 28.0 | 12582995 | 4063949 | 32.3% |

**Slowdown:** 5.18×

### Test 4 — Branch vs Branchless

Same sum, computed via conditional jump vs. arithmetic mask, on sorted and random data.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sorted + branch | 119.3 | 536871153 | 289 | 0.0% |
| Sorted + branchless | 167.4 | 268435742 | 88 | 0.0% |
| Random + branch | 847.4 | 536871893 | 134252303 | 25.0% |
| Random + branchless | 167.4 | 268435744 | 81 | 0.0% |

**Branch penalty on random data:** 7.11× vs sorted-branch

**Branchless is consistent:** 1.00× random vs sorted

### Interpretation guide

```
Misprediction penalty = pipeline flush + refill latency.
On typical x86 out-of-order CPUs this is ~15-20 cycles.
At 50% miss rate on 1M branches/rep: ~8M wasted cycles/rep.
At 4 GHz that is ~2 ms overhead per rep — matches Test 1.

Tests 1/2: direct branches (conditional jumps in the loop).
Test 3:    indirect branches (call via register / BTB);
           penalty per miss is often higher than direct.
Test 4:    branchless mask trick eliminates branches entirely;
           consistent throughput regardless of data order.
```

## AMD Ryzen 5 9600X 6-Core Processor @ 5.50 GHz (WSL2)

_2026-08-21_

| Array size | Repetitions | Perf counters |
|---:|---:|---|
| 4194304 elements | 64/trial | available (hardware counts) |

### Test 1 — Threshold Sum

Sorted vs shuffled array, same direct branch. Identical data, different order.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sorted (predictable, ~0% misses) | 50.8 | 536871085 | 1543 | 0.0% |
| Shuffled (unpredictable, ~50% misses) | 656.3 | 536871714 | 134161142 | 25.0% |

**Slowdown:** 12.91×

### Test 2 — Stride Conditional

Both arrays have ~25% ones; only the pattern differs.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Periodic (every 4th — learnable) | 53.7 | 536871088 | 6230 | 0.0% |
| Random (same rate — unlearnable) | 390.9 | 536871441 | 71468641 | 13.3% |

**Slowdown:** 7.28×

### Test 3 — Indirect Dispatch

32 targets, function-pointer call. BTB must predict the target address.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sequential i%32 (BTB learns cycle) | 4.6 | 12582975 | 254 | 0.0% |
| Random index (BTB always wrong) | 28.9 | 12582998 | 4063451 | 32.3% |

**Slowdown:** 6.26×

### Test 4 — Branch vs Branchless

Same sum, computed via conditional jump vs. arithmetic mask, on sorted and random data.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sorted + branch | 50.3 | 536871086 | 1240 | 0.0% |
| Sorted + branchless | 66.6 | 268435647 | 625 | 0.0% |
| Random + branch | 655.9 | 536871730 | 134191545 | 25.0% |
| Random + branchless | 66.5 | 268435643 | 839 | 0.0% |

**Branch penalty on random data:** 13.03× vs sorted-branch

**Branchless is consistent:** 1.00× random vs sorted

### Interpretation guide

```
Misprediction penalty = pipeline flush + refill latency.
On typical x86 out-of-order CPUs this is ~15-20 cycles.
At 50% miss rate on 1M branches/rep: ~8M wasted cycles/rep.
At 4 GHz that is ~2 ms overhead per rep — matches Test 1.

Tests 1/2: direct branches (conditional jumps in the loop).
Test 3:    indirect branches (call via register / BTB);
           penalty per miss is often higher than direct.
Test 4:    branchless mask trick eliminates branches entirely;
           consistent throughput regardless of data order.
```

## ARM Cortex-A72 @ 1.60 GHz (Raspberry Pi 4 Model B Rev 1.5)

_2026-08-21_

| Array size | Repetitions | Perf counters |
|---:|---:|---|
| 4194304 elements | 64/trial | available (hardware counts) |

### Test 1 — Threshold Sum

Sorted vs shuffled array, same direct branch. Identical data, different order.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sorted (predictable, ~0% misses) | 541.8 | 536872982 | 703 | 0.0% |
| Shuffled (unpredictable, ~50% misses) | 2231.4 | 634350960 | 134100010 | 21.1% |

**Slowdown:** 4.12×

### Test 2 — Stride Conditional

Both arrays have ~25% ones; only the pattern differs.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Periodic (every 4th — learnable) | 801.3 | 536872469 | 407 | 0.0% |
| Random (same rate — unlearnable) | 1655.0 | 597956846 | 84984614 | 14.2% |

**Slowdown:** 2.07×

### Test 3 — Indirect Dispatch

32 targets, function-pointer call. BTB must predict the target address.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sequential i%32 (BTB learns cycle) | 61.1 | 12583178 | 3670054 | 29.2% |
| Random index (BTB always wrong) | 77.4 | 16800922 | 4063772 | 24.2% |

**Slowdown:** 1.27×

### Test 4 — Branch vs Branchless

Same sum, computed via conditional jump vs. arithmetic mask, on sorted and random data.

| Variant | Time (ms) | Branches | Misses | Miss % |
|---|---:|---:|---:|---:|
| Sorted + branch | 537.6 | 536872627 | 524 | 0.0% |
| Sorted + branchless | 554.2 | 268435958 | 89 | 0.0% |
| Random + branch | 2231.2 | 634450951 | 134232670 | 21.2% |
| Random + branchless | 555.3 | 268435982 | 79 | 0.0% |

**Branch penalty on random data:** 4.15× vs sorted-branch

**Branchless is consistent:** 1.00× random vs sorted

### Interpretation guide

```
Misprediction penalty = pipeline flush + refill latency.
On typical x86 out-of-order CPUs this is ~15-20 cycles.
At 50% miss rate on 1M branches/rep: ~8M wasted cycles/rep.
At 4 GHz that is ~2 ms overhead per rep — matches Test 1.

Tests 1/2: direct branches (conditional jumps in the loop).
Test 3:    indirect branches (call via register / BTB);
           penalty per miss is often higher than direct.
Test 4:    branchless mask trick eliminates branches entirely;
           consistent throughput regardless of data order.
```
