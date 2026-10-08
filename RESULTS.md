# NEW RESULTS

## Intel(R) Core(TM) i5-8350U CPU @ 3.60 GHz

_2026-10-08_

| Array size | Repetitions | Perf counters |
|---:|---:|---|
| 4194304 elements | 64/trial | available (hardware counts) |

_Median of 6 trials per variant, after one warm-up run. Range is the fastest–slowest trial._

### Test 1 — Threshold Sum

Sorted vs shuffled array, same direct branch. Identical data, different order.

| Variant | Time (ms) | Range (ms) | Cycles | Branches | Misses | Miss % | Misses/elem |
|---|---:|---:|---:|---:|---:|---:|---:|
| Sorted (predictable, ~0% misses) | 300.4 | 292.4–305.2 | 673346941 | 536871332 | 1043 | 0.0% | 0.000 |
| Shuffled (unpredictable, ~50% misses) | 1648.5 | 1612.9–1673.3 | 3682259659 | 536872743 | 134132630 | 25.0% | 0.500 |

**Slowdown:** 5.49×

**Cost per miss:** 22.4 cycles

### Test 2 — Stride Conditional

Both arrays have ~25% ones; only the pattern differs.

| Variant | Time (ms) | Range (ms) | Cycles | Branches | Misses | Miss % | Misses/elem |
|---|---:|---:|---:|---:|---:|---:|---:|
| Periodic (every 4th — learnable) | 377.0 | 367.0–381.7 | 850914343 | 536871411 | 64365 | 0.0% | 0.000 |
| Random (same rate — unlearnable) | 1172.9 | 1152.3–1187.0 | 2596364257 | 536872240 | 76149029 | 14.2% | 0.284 |

**Slowdown:** 3.11×

**Cost per miss:** 22.9 cycles

### Test 3 — Indirect Dispatch

32 targets, function-pointer call. The indirect predictor must guess the target address.

| Variant | Time (ms) | Range (ms) | Cycles | Branches | Misses | Miss % | Misses/call |
|---|---:|---:|---:|---:|---:|---:|---:|
| Sequential i%32 (learnable cycle) | 12.4 | 10.3–13.5 | 28117738 | 12582967 | 221935 | 1.8% | 0.053 |
| Random index (unlearnable) | 54.7 | 54.4–58.0 | 123310873 | 12583010 | 4063622 | 32.3% | 0.969 |

**Slowdown:** 4.41×

**Cost per miss:** 24.8 cycles

### Test 4 — Branch vs Branchless

Same sum, computed via conditional jump vs. arithmetic mask, on sorted and random data.

| Variant | Time (ms) | Range (ms) | Cycles | Branches | Misses | Miss % | Misses/elem |
|---|---:|---:|---:|---:|---:|---:|---:|
| Sorted + branch | 300.8 | 299.2–343.0 | 676273298 | 536871334 | 1101 | 0.0% | 0.000 |
| Sorted + branchless | 277.7 | 256.6–339.9 | 605999951 | 268435858 | 647 | 0.0% | 0.000 |
| Random + branch | 1628.2 | 1607.8–1675.0 | 3681711679 | 536872710 | 134128663 | 25.0% | 0.500 |
| Random + branchless | 259.5 | 247.9–274.8 | 577578644 | 268435825 | 563 | 0.0% | 0.000 |

**Branch penalty on random data:** 5.41× vs sorted-branch

**Branchless is consistent:** 0.93× random vs sorted

**Cost per miss:** 22.4 cycles

**Average clock during measurements:** 2.23 GHz

# OLD RESULTS

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
