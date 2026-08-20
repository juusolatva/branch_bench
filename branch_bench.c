/*
 * branch_bench.c — CPU Branch Prediction Efficiency Benchmark
 *
 * Four tests that contrast highly predictable vs unpredictable branching
 * to measure the real CPU cost of branch misprediction:
 *
 *  Test 1 — Threshold Sum       sorted vs shuffled array              (classic direct-branch demo)
 *  Test 2 — Stride Conditional  periodic 25% vs random 25% take-rate  (repeating pattern stress)
 *  Test 3 — Indirect Dispatch   sequential vs random function pointer  (BTB / indirect predictor)
 *  Test 4 — Branch vs Branchless sorted/random × branch/arithmetic    (misprediction cost isolation)
 *
 * On Linux, hardware counters are read via perf_event_open(2) when the kernel
 * allows it (most KVM guests support this).  Falls back to wall-clock timing
 * automatically, which is meaningful everywhere including containers and VMs.
 *
 * Build:
 *   gcc  -O2 -fno-tree-vectorize -fno-if-conversion -o branch_bench branch_bench.c
 *   clang -O2 -fno-tree-vectorize -fno-if-conversion -o branch_bench branch_bench.c
 *
 * Note: -fno-tree-vectorize prevents SIMD rewriting that would bypass branches.
 *       -fno-if-conversion keeps if-else as actual branch instructions (not CMOV),
 *       which is required for tests 1-3 to show misprediction cost.
 *       Test 4's branchless variant uses arithmetic masks — unaffected by these flags.
 *
 * Run:
 *   ./branch_bench
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

/* ── Linux perf_event support (optional) ────────────────────────────────── */
#ifdef __linux__
#  include <unistd.h>
#  include <sys/syscall.h>
#  ifdef __NR_perf_event_open
#    include <linux/perf_event.h>
#    include <sys/ioctl.h>
#    define HAVE_PERF 1
#  endif
#endif

/* ── Tuneable parameters ─────────────────────────────────────────────────── */
#define ARRAY_LEN     (1u << 22)   /* 1 048 576 elements for tests 1/2/4    */
#define REPS          64           /* outer repetitions (tests 1/2/4)        */
#define THRESHOLD     128          /* byte comparison threshold               */
#define NUM_FUNCS     32           /* distinct indirect call targets (test 3) */
#define DISPATCH_N    (1u << 22)   /* total indirect calls per trial (test 3) */

/* ── Types ───────────────────────────────────────────────────────────────── */
typedef unsigned long long u64;
typedef unsigned char      u8;

typedef struct {
    double time_ms;
    u64    branch_total;   /* 0 if perf unavailable   */
    u64    branch_misses;
    u64    result;         /* anti-dead-code-elimination sink */
} Result;

/* ─────────────────────────────────────────────────────────────────────────
 * Timing
 * ───────────────────────────────────────────────────────────────────────── */
static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Perf counters  (no-ops when unavailable)
 * ───────────────────────────────────────────────────────────────────────── */
#ifdef HAVE_PERF
static int pfd_branches = -1;
static int pfd_misses   = -1;

static int perf_open(uint32_t type, uint64_t config)
{
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(pe));
    pe.type           = type;
    pe.size           = sizeof(pe);
    pe.config         = config;
    pe.disabled       = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv     = 1;
    return (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
}

static void perf_init(void)
{
    /* Try standard generic hardware events first */
    pfd_branches = perf_open(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
    pfd_misses   = perf_open(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);

#if defined(__aarch64__) || defined(__arm__)
    /* Fallback: ARM Cortex-A72 raw hardware PMU event codes */
    if (pfd_branches < 0) {
        pfd_branches = perf_open(PERF_TYPE_RAW, 0x12); /* BR_PRED: branch executed */
    }
    if (pfd_misses < 0) {
        pfd_misses = perf_open(PERF_TYPE_RAW, 0x21);   /* BR_MIS_PRED: mispredicted branch */
    }
#endif
}

static void perf_start(void)
{
    if (pfd_branches >= 0) {
        ioctl(pfd_branches, PERF_EVENT_IOC_RESET,  0);
        ioctl(pfd_branches, PERF_EVENT_IOC_ENABLE, 0);
    }
    if (pfd_misses >= 0) {
        ioctl(pfd_misses, PERF_EVENT_IOC_RESET,  0);
        ioctl(pfd_misses, PERF_EVENT_IOC_ENABLE, 0);
    }
}

static void perf_stop(Result *r)
{
    if (pfd_branches >= 0) {
        ioctl(pfd_branches, PERF_EVENT_IOC_DISABLE, 0);
        read(pfd_branches, &r->branch_total, sizeof(u64));
    }
    if (pfd_misses >= 0) {
        ioctl(pfd_misses, PERF_EVENT_IOC_DISABLE, 0);
        read(pfd_misses, &r->branch_misses, sizeof(u64));
    }
}

static int perf_available(void) { return pfd_branches >= 0 || pfd_misses >= 0; }

#else
static void perf_init(void)       {}
static void perf_start(void)      {}
static void perf_stop(Result *r)  { (void)r; }
static int  perf_available(void)  { return 0; }
#endif

/* ─────────────────────────────────────────────────────────────────────────
 * Fast RNG — xorshift64, avoids stdlib rand() overhead and bias
 * ───────────────────────────────────────────────────────────────────────── */
static u64 rng_state = 0xdeadbeefcafe1234ULL;

static inline u64 rng64(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Array utilities
 * ───────────────────────────────────────────────────────────────────────── */
static int cmp_u8_asc(const void *a, const void *b)
{
    return (int)*(const u8*)a - (int)*(const u8*)b;
}

static void shuffle_u8(u8 *arr, size_t n)
{
    for (size_t i = n - 1; i > 0; --i) {
        size_t j = rng64() % (i + 1);
        u8 tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *
 *  TEST 1 — Threshold Sum: Sorted vs Shuffled Array
 *
 *  Accumulates all elements > THRESHOLD.  Both arrays contain identical data,
 *  just in different orders.
 *
 *  Sorted:   the branch outcome is "not-taken" for the first ~50% of the
 *            array, then "always-taken" for the rest.  The predictor finds
 *            the crossover within a few iterations and then has near-zero
 *            mispredictions.
 *
 *  Shuffled: each element's outcome is independent and ~50/50.  The predictor
 *            can do no better than chance, yielding ~50% misprediction rate.
 *
 * ═══════════════════════════════════════════════════════════════════════════ */
__attribute__((noinline))
static u64 sum_threshold(const u8 *arr, size_t n)
{
    u64 acc = 0;
    for (int rep = 0; rep < REPS; rep++)
        for (size_t i = 0; i < n; i++)
            if (arr[i] > THRESHOLD)   /* <── THE branch being predicted */
                acc += arr[i];
    return acc;
}

static void run_test1(void)
{
    u8 *sorted   = malloc(ARRAY_LEN);
    u8 *shuffled = malloc(ARRAY_LEN);
    if (!sorted || !shuffled) { perror("malloc"); exit(1); }

    /* Fill with uniform random bytes, then sort one copy */
    for (size_t i = 0; i < ARRAY_LEN; i++)
        sorted[i] = (u8)(rng64() & 0xff);
    memcpy(shuffled, sorted, ARRAY_LEN);
    qsort(sorted, ARRAY_LEN, 1, cmp_u8_asc);
    shuffle_u8(shuffled, ARRAY_LEN);

    printf("TEST 1 — Threshold Sum  (sorted vs shuffled array, %u×%u reps)\n",
           ARRAY_LEN, REPS);
    printf("  Counts/sums elements > %d.  Identical data, different order.\n\n",
           THRESHOLD);

    Result rs = {0}, rr = {0};
    double t;

    t = now_ms(); perf_start();
    rs.result = sum_threshold(sorted, ARRAY_LEN);
    perf_stop(&rs); rs.time_ms = now_ms() - t;

    t = now_ms(); perf_start();
    rr.result = sum_threshold(shuffled, ARRAY_LEN);
    perf_stop(&rr); rr.time_ms = now_ms() - t;

    free(sorted); free(shuffled);

    printf("  %-38s  %7.1f ms", "Sorted   (predictable, ~0% misses)", rs.time_ms);
    if (perf_available() && rs.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               rs.branch_total, rs.branch_misses,
               100.0 * rs.branch_misses / rs.branch_total);
    printf("\n");

    printf("  %-38s  %7.1f ms", "Shuffled (unpredictable, ~50% misses)", rr.time_ms);
    if (perf_available() && rr.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               rr.branch_total, rr.branch_misses,
               100.0 * rr.branch_misses / rr.branch_total);
    printf("\n");

    printf("  → Slowdown:  %.2f×\n\n", rr.time_ms / rs.time_ms);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *
 *  TEST 2 — Stride Conditional: Periodic vs Random Same-Rate Pattern
 *
 *  Counts how many array elements pass a condition.  Both variants have the
 *  same ~25% branch-taken rate, but one is perfectly periodic and the other
 *  is random.
 *
 *  Periodic ("every 4th"):  the predictor learns the T-N-N-N-T-N-N-N cycle
 *                           within the first few passes → nearly zero misses.
 *
 *  Random 25%:              a lookup table of pre-randomised decisions
 *                           (generated offline so RNG overhead is excluded).
 *                           Same take-rate but no exploitable pattern.
 *
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Pre-generated branch decision arrays (avoids RNG in hot loop) */
static u8 *decisions_periodic = NULL;  /* 1 at every 4th position           */
static u8 *decisions_random   = NULL;  /* 1 with 25% probability, random    */

__attribute__((noinline))
static u64 count_decisions(const u8 *decisions, size_t n)
{
    u64 acc = 0;
    for (int rep = 0; rep < REPS; rep++)
        for (size_t i = 0; i < n; i++)
            if (decisions[i])   /* branch outcome determined by table */
                acc++;
    return acc;
}

static void run_test2(void)
{
    decisions_periodic = malloc(ARRAY_LEN);
    decisions_random   = malloc(ARRAY_LEN);
    if (!decisions_periodic || !decisions_random) { perror("malloc"); exit(1); }

    for (size_t i = 0; i < ARRAY_LEN; i++) {
        decisions_periodic[i] = ((i & 3) == 0) ? 1 : 0;
        decisions_random[i]   = ((rng64() % 4) == 0) ? 1 : 0;
    }

    printf("TEST 2 — Stride Conditional  (periodic vs random 25%%, %u×%u reps)\n",
           ARRAY_LEN, REPS);
    printf("  Both arrays have ~25%% ones; only the pattern differs.\n\n");

    Result rp = {0}, rr = {0};
    double t;

    t = now_ms(); perf_start();
    rp.result = count_decisions(decisions_periodic, ARRAY_LEN);
    perf_stop(&rp); rp.time_ms = now_ms() - t;

    t = now_ms(); perf_start();
    rr.result = count_decisions(decisions_random, ARRAY_LEN);
    perf_stop(&rr); rr.time_ms = now_ms() - t;

    free(decisions_periodic); decisions_periodic = NULL;
    free(decisions_random);   decisions_random   = NULL;

    printf("  %-38s  %7.1f ms", "Periodic (every 4th — learnable)", rp.time_ms);
    if (perf_available() && rp.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               rp.branch_total, rp.branch_misses,
               100.0 * rp.branch_misses / rp.branch_total);
    printf("\n");

    printf("  %-38s  %7.1f ms", "Random   (same rate — unlearnable)", rr.time_ms);
    if (perf_available() && rr.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               rr.branch_total, rr.branch_misses,
               100.0 * rr.branch_misses / rr.branch_total);
    printf("\n");

    printf("  → Slowdown:  %.2f×\n\n", rr.time_ms / rp.time_ms);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *
 *  TEST 3 — Indirect Branch Dispatch: Sequential vs Random  (BTB stress)
 *
 *  Calls one of NUM_FUNCS trivial leaf functions via a function-pointer table.
 *  The CPU's Branch Target Buffer (BTB) and indirect-branch predictor try to
 *  guess the *target address* of each indirect call, not just taken/not-taken.
 *
 *  Sequential (i % NUM_FUNCS):  the predictor sees a repeating cycle of 32
 *                                targets and learns it quickly → few misses.
 *
 *  Random:                      the next target is drawn from a pre-shuffled
 *                               array of random indices → BTB can't predict
 *                               → miss on almost every call.
 *
 *  This mirrors stress-ng's --branch and --icache stressors.
 *
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Generate NUM_FUNCS distinct leaf functions via X-macro ─────────────────── */
typedef u64 (*leaf_fn_t)(u64);

#define FOR_EACH_LEAF(F) \
    F( 0) F( 1) F( 2) F( 3) F( 4) F( 5) F( 6) F( 7) \
    F( 8) F( 9) F(10) F(11) F(12) F(13) F(14) F(15) \
    F(16) F(17) F(18) F(19) F(20) F(21) F(22) F(23) \
    F(24) F(25) F(26) F(27) F(28) F(29) F(30) F(31)

/* Each function does one unique arithmetic op so it can't be merged by the
 * linker's identical-code folding (ICF). The constant is a unique Weyl-
 * sequence step derived from the golden ratio. */
#define DEF_LEAF(n) \
    __attribute__((noinline)) \
    static u64 leaf_##n(u64 x) { return x ^ ((u64)(n) * 0x9e3779b97f4a7c15ULL); }

FOR_EACH_LEAF(DEF_LEAF)

#define PTR_LEAF(n) leaf_##n,
static leaf_fn_t leaf_table[NUM_FUNCS] = { FOR_EACH_LEAF(PTR_LEAF) };

/* Pre-built random index sequence (uint8_t → 1 byte/call → cache-friendly) */
static u8 *dispatch_indices = NULL;

__attribute__((noinline))
static u64 dispatch_sequential(size_t n)
{
    u64 acc = 0;
    for (size_t i = 0; i < n; i++)
        acc = leaf_table[i % NUM_FUNCS](acc);
    return acc;
}

__attribute__((noinline))
static u64 dispatch_random(const u8 *idx, size_t n)
{
    u64 acc = 0;
    for (size_t i = 0; i < n; i++)
        acc = leaf_table[idx[i]](acc);
    return acc;
}

static void run_test3(void)
{
    dispatch_indices = malloc(DISPATCH_N);
    if (!dispatch_indices) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < DISPATCH_N; i++)
        dispatch_indices[i] = (u8)(rng64() % NUM_FUNCS);

    printf("TEST 3 — Indirect Dispatch  (%u targets, %uM calls)\n",
           NUM_FUNCS, DISPATCH_N >> 20);
    printf("  Calls leaf functions via pointer. BTB must predict the target address.\n\n");

    Result rs = {0}, rr = {0};
    double t;

    t = now_ms(); perf_start();
    rs.result = dispatch_sequential(DISPATCH_N);
    perf_stop(&rs); rs.time_ms = now_ms() - t;

    t = now_ms(); perf_start();
    rr.result = dispatch_random(dispatch_indices, DISPATCH_N);
    perf_stop(&rr); rr.time_ms = now_ms() - t;

    free(dispatch_indices); dispatch_indices = NULL;

    printf("  %-38s  %7.1f ms", "Sequential i%32 (BTB learns cycle)", rs.time_ms);
    if (perf_available() && rs.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               rs.branch_total, rs.branch_misses,
               100.0 * rs.branch_misses / rs.branch_total);
    printf("\n");

    printf("  %-38s  %7.1f ms", "Random index (BTB always wrong)", rr.time_ms);
    if (perf_available() && rr.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               rr.branch_total, rr.branch_misses,
               100.0 * rr.branch_misses / rr.branch_total);
    printf("\n");

    printf("  → Slowdown:  %.2f×\n\n", rr.time_ms / rs.time_ms);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *
 *  TEST 4 — Branch vs Branchless Computation  (misprediction cost isolation)
 *
 *  Both variants compute the same result: sum of elements > THRESHOLD.
 *  Tested on sorted data (predictable) and random data (unpredictable).
 *
 *  Branch version:
 *      if (arr[i] > T) acc += arr[i];
 *    The compiler emits a conditional jump.  On random data this mispredicts
 *    ~50% of the time.
 *
 *  Branchless version (arithmetic mask):
 *      mask = -(u64)(arr[i] > T);   // 0 or 0xFFFF...FFFF
 *      acc += arr[i] & mask;
 *    No conditional jump at all — the comparison result is a 0/1 integer
 *    that is sign-extended into a bitmask.  Performance is the same regardless
 *    of data distribution.
 *
 *  Expected pattern:
 *      Sorted   + branch    ≈ Sorted   + branchless  (few misses anyway)
 *      Random   + branch    >> Random  + branchless  (misprediction penalty)
 *
 *  This directly quantifies the penalty per misprediction and shows why
 *  modern compilers emit CMOV/branchless code for data-independent conditions.
 *
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Branch version — kept as a branch by -fno-if-conversion build flag */
__attribute__((noinline))
static u64 sum_branch(const u8 *arr, size_t n)
{
    u64 acc = 0;
    for (int rep = 0; rep < REPS; rep++)
        for (size_t i = 0; i < n; i++)
            if (arr[i] > THRESHOLD)
                acc += arr[i];
    return acc;
}

/* Branchless version — arithmetic mask, no conditional jump possible */
__attribute__((noinline))
static u64 sum_branchless(const u8 *arr, size_t n)
{
    u64 acc = 0;
    for (int rep = 0; rep < REPS; rep++)
        for (size_t i = 0; i < n; i++) {
            u64 mask = -(u64)(arr[i] > THRESHOLD);  /* 0 or ~0ULL */
            acc += arr[i] & mask;
        }
    return acc;
}

static void run_test4(void)
{
    u8 *sorted   = malloc(ARRAY_LEN);
    u8 *random_d = malloc(ARRAY_LEN);
    if (!sorted || !random_d) { perror("malloc"); exit(1); }

    for (size_t i = 0; i < ARRAY_LEN; i++)
        sorted[i] = (u8)(rng64() & 0xff);
    qsort(sorted, ARRAY_LEN, 1, cmp_u8_asc);

    for (size_t i = 0; i < ARRAY_LEN; i++)
        random_d[i] = (u8)(rng64() & 0xff);

    printf("TEST 4 — Branch vs Branchless  (sorted and random data, %u×%u reps)\n",
           ARRAY_LEN, REPS);
    printf("  Branchless uses arithmetic mask: -(u64)(v > T) to avoid jumps.\n\n");

    Result r_sb = {0}, r_sl = {0}, r_rb = {0}, r_rl = {0};
    double t;

    t = now_ms(); perf_start();
    r_sb.result = sum_branch(sorted, ARRAY_LEN);
    perf_stop(&r_sb); r_sb.time_ms = now_ms() - t;

    t = now_ms(); perf_start();
    r_sl.result = sum_branchless(sorted, ARRAY_LEN);
    perf_stop(&r_sl); r_sl.time_ms = now_ms() - t;

    t = now_ms(); perf_start();
    r_rb.result = sum_branch(random_d, ARRAY_LEN);
    perf_stop(&r_rb); r_rb.time_ms = now_ms() - t;

    t = now_ms(); perf_start();
    r_rl.result = sum_branchless(random_d, ARRAY_LEN);
    perf_stop(&r_rl); r_rl.time_ms = now_ms() - t;

    free(sorted); free(random_d);

    /* verify both produce same result on the same data (sanity check) */
    if (r_sb.result != r_sl.result || r_rb.result != r_rl.result)
        fprintf(stderr, "  WARNING: branch/branchless results differ!\n");

    printf("  %-38s  %7.1f ms", "Sorted  + branch",     r_sb.time_ms);
    if (perf_available() && r_sb.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               r_sb.branch_total, r_sb.branch_misses,
               100.0 * r_sb.branch_misses / r_sb.branch_total);
    printf("\n");

    printf("  %-38s  %7.1f ms", "Sorted  + branchless", r_sl.time_ms);
    if (perf_available() && r_sl.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               r_sl.branch_total, r_sl.branch_misses,
               100.0 * r_sl.branch_misses / r_sl.branch_total);
    printf("\n");

    printf("  %-38s  %7.1f ms", "Random  + branch",     r_rb.time_ms);
    if (perf_available() && r_rb.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               r_rb.branch_total, r_rb.branch_misses,
               100.0 * r_rb.branch_misses / r_rb.branch_total);
    printf("\n");

    printf("  %-38s  %7.1f ms", "Random  + branchless", r_rl.time_ms);
    if (perf_available() && r_rl.branch_total)
        printf("  branches %10"PRIu64"  misses %9"PRIu64"  (%.1f%%)",
               r_rl.branch_total, r_rl.branch_misses,
               100.0 * r_rl.branch_misses / r_rl.branch_total);
    printf("\n");

    printf("\n  Branch penalty on random data:  %.2f× vs sorted-branch\n",
           r_rb.time_ms / r_sb.time_ms);
    printf("  Branchless is consistent:       %.2f× random vs sorted\n\n",
           r_rl.time_ms / r_sl.time_ms);
}

/* ─────────────────────────────────────────────────────────────────────────
 * main
 * ───────────────────────────────────────────────────────────────────────── */
int main(void)
{
    perf_init();

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  CPU Branch Prediction Benchmark\n");
    printf("  Array size:      %10u elements\n", ARRAY_LEN);
    printf("  Repetitions:     %10u per trial\n", REPS);
    printf("  Perf counters:   %s\n",
           perf_available() ? "AVAILABLE (hardware branch-miss counts shown)"
                            : "unavailable — showing wall-clock timing only");
    printf("══════════════════════════════════════════════════════════════\n\n");

    run_test1();
    run_test2();
    run_test3();
    run_test4();

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  Interpretation guide\n");
    printf("  ─────────────────────────────────────────────────────────\n");
    printf("  Misprediction penalty = pipeline flush + refill latency.\n");
    printf("  On typical x86 out-of-order CPUs this is ~15–20 cycles.\n");
    printf("  At 50%% miss rate on 1M branches/rep: ~8M wasted cycles/rep.\n");
    printf("  At 4 GHz that is ~2 ms overhead per rep — matches Test 1.\n");
    printf("\n");
    printf("  Tests 1/2: direct branches (conditional jumps in the loop).\n");
    printf("  Test 3:    indirect branches (call via register / BTB);\n");
    printf("             penalty per miss is often higher than direct.\n");
    printf("  Test 4:    branchless mask trick eliminates branches entirely;\n");
    printf("             consistent throughput regardless of data order.\n");
    printf("══════════════════════════════════════════════════════════════\n");

#ifdef HAVE_PERF
    if (pfd_branches >= 0) close(pfd_branches);
    if (pfd_misses   >= 0) close(pfd_misses);
#endif

    return 0;
}
