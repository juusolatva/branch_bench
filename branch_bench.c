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
typedef uint64_t        u64;
typedef unsigned char   u8;

typedef struct {
    double time_ms;
    u64    branch_total;   /* 0 if perf unavailable   */
    u64    branch_misses;
    u64    result;         /* anti-dead-code-elimination sink */
} Result;

/* Forces the optimizer to treat v as observed, so the computation that
 * produced it can't be proven dead and elided. Use whenever a test's
 * results aren't already consumed by something with observable side
 * effects (e.g. a printed sanity check). */
static void anti_dce_sink(u64 v)
{
    volatile u64 sink = v;
    (void)sink;
}

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
 *
 * Call convention: perf_start() is called just *before* the now_ms() timer
 * starts, and perf_stop() just *after* it stops, so the ioctl/read syscalls
 * themselves are never counted in time_ms. This means the perf window is
 * very slightly wider than the timed window — branch_total/branch_misses
 * can include a handful of instructions time_ms doesn't. Negligible in
 * magnitude (a few syscalls' worth out of millions of branches), and
 * intentional: the alternative is syscall overhead leaking into time_ms.
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
        if (read(pfd_branches, &r->branch_total, sizeof(u64)) != sizeof(u64)) {
            r->branch_total = 0;
        }
    }
    if (pfd_misses >= 0) {
        ioctl(pfd_misses, PERF_EVENT_IOC_DISABLE, 0);
        if (read(pfd_misses, &r->branch_misses, sizeof(u64)) != sizeof(u64)) {
            r->branch_misses = 0;
        }
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
 * Formatted report output (-o/--output)
 *
 * The console output above is meant to be read as it scrolls by. This
 * writes the same numbers to a Markdown file as proper tables, so results
 * can be dropped straight into RESULTS.md instead of pasted as a raw
 * terminal transcript.
 * ───────────────────────────────────────────────────────────────────────── */
static void get_cpu_model(char *buf, size_t bufsz)
{
    snprintf(buf, bufsz, "Unknown CPU");
#ifdef __linux__
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* x86: "model name\t: ..."   ARM: "Model\t: ..." (falls back to
         * "Hardware" on some older ARM kernels if "Model" is absent) */
        if (strncmp(line, "model name", 10) == 0 ||
            strncmp(line, "Model", 5) == 0 ||
            strncmp(line, "Hardware", 8) == 0) {
            char *colon = strchr(line, ':');
            if (!colon) continue;
            colon++;
            while (*colon == ' ' || *colon == '\t') colon++;
            size_t len = strlen(colon);
            while (len && (colon[len-1] == '\n' || colon[len-1] == '\r'))
                colon[--len] = '\0';
            if (len) {
                snprintf(buf, bufsz, "%s", colon);
                if (strncmp(line, "model name", 10) == 0)
                    break;  /* prefer x86 "model name" over any later match */
            }
        }
    }
    fclose(f);
#endif
}

typedef struct {
    const char *label;
    Result      r;
} ReportRow;

/* Emits one Markdown table (variant × time/branches/misses/miss%) for a
 * test, given its already-computed Result rows. */
static void report_table(FILE *f, const char *title, const char *desc,
                          const ReportRow *rows, int n)
{
    if (!f) return;
    fprintf(f, "### %s\n\n", title);
    if (desc) fprintf(f, "%s\n\n", desc);
    fprintf(f, "| Variant | Time (ms) | Branches | Misses | Miss %% |\n");
    fprintf(f, "|---|---:|---:|---:|---:|\n");
    for (int i = 0; i < n; i++) {
        const Result *r = &rows[i].r;
        fprintf(f, "| %s | %.1f |", rows[i].label, r->time_ms);
        if (perf_available() && r->branch_total)
            fprintf(f, " %" PRIu64 " | %" PRIu64 " | %.1f%% |\n",
                    r->branch_total, r->branch_misses,
                    100.0 * r->branch_misses / r->branch_total);
        else
            fprintf(f, " – | – | – |\n");
    }
    fprintf(f, "\n");
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

static void run_test1(FILE *report)
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

    perf_start();
    t = now_ms();
    rs.result = sum_threshold(sorted, ARRAY_LEN);
    rs.time_ms = now_ms() - t;
    perf_stop(&rs);

    perf_start();
    t = now_ms();
    rr.result = sum_threshold(shuffled, ARRAY_LEN);
    rr.time_ms = now_ms() - t;
    perf_stop(&rr);

    anti_dce_sink(rs.result + rr.result);

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

    ReportRow rows[] = {
        { "Sorted (predictable, ~0% misses)",     rs },
        { "Shuffled (unpredictable, ~50% misses)", rr },
    };
    report_table(report, "Test 1 — Threshold Sum",
                 "Sorted vs shuffled array, same direct branch. Identical data, different order.",
                 rows, 2);
    if (report) fprintf(report, "**Slowdown:** %.2f×\n\n", rr.time_ms / rs.time_ms);
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

static void run_test2(FILE *report)
{
    decisions_periodic = malloc(ARRAY_LEN);
    decisions_random   = malloc(ARRAY_LEN);
    if (!decisions_periodic || !decisions_random) { perror("malloc"); exit(1); }

    for (size_t i = 0; i < ARRAY_LEN; i++) {
        decisions_periodic[i] = ((i & 3) == 0) ? 1 : 0;
        decisions_random[i] = (((rng64() >> 32) % 4) == 0) ? 1 : 0;
    }

    printf("TEST 2 — Stride Conditional  (periodic vs random 25%%, %u×%u reps)\n",
           ARRAY_LEN, REPS);
    printf("  Both arrays have ~25%% ones; only the pattern differs.\n\n");

    Result rp = {0}, rr = {0};
    double t;

    perf_start();
    t = now_ms();
    rp.result = count_decisions(decisions_periodic, ARRAY_LEN);
    rp.time_ms = now_ms() - t;
    perf_stop(&rp);

    perf_start();
    t = now_ms();
    rr.result = count_decisions(decisions_random, ARRAY_LEN);
    rr.time_ms = now_ms() - t;
    perf_stop(&rr);

    anti_dce_sink(rp.result + rr.result);

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

    ReportRow rows[] = {
        { "Periodic (every 4th — learnable)",  rp },
        { "Random (same rate — unlearnable)",  rr },
    };
    report_table(report, "Test 2 — Stride Conditional",
                 "Both arrays have ~25% ones; only the pattern differs.",
                 rows, 2);
    if (report) fprintf(report, "**Slowdown:** %.2f×\n\n", rr.time_ms / rp.time_ms);
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

static void run_test3(FILE *report)
{
    dispatch_indices = malloc(DISPATCH_N);
    if (!dispatch_indices) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < DISPATCH_N; i++)
        dispatch_indices[i] = (u8)((rng64() >> 32) % NUM_FUNCS);

    printf("TEST 3 — Indirect Dispatch  (%u targets, %uM calls)\n",
           NUM_FUNCS, DISPATCH_N >> 20);
    printf("  Calls leaf functions via pointer. BTB must predict the target address.\n\n");

    Result rs = {0}, rr = {0};
    double t;

    perf_start();
    t = now_ms();
    rs.result = dispatch_sequential(DISPATCH_N);
    rs.time_ms = now_ms() - t;
    perf_stop(&rs);

    perf_start();
    t = now_ms();
    rr.result = dispatch_random(dispatch_indices, DISPATCH_N);
    rr.time_ms = now_ms() - t;
    perf_stop(&rr);

    anti_dce_sink(rs.result + rr.result);

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

    ReportRow rows[] = {
        { "Sequential i%32 (BTB learns cycle)", rs },
        { "Random index (BTB always wrong)",    rr },
    };
    report_table(report, "Test 3 — Indirect Dispatch",
                 "32 targets, function-pointer call. BTB must predict the target address.",
                 rows, 2);
    if (report) fprintf(report, "**Slowdown:** %.2f×\n\n", rr.time_ms / rs.time_ms);
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

static void run_test4(FILE *report)
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

    perf_start();
    t = now_ms();
    r_sb.result = sum_branch(sorted, ARRAY_LEN);
    r_sb.time_ms = now_ms() - t;
    perf_stop(&r_sb);

    perf_start();
    t = now_ms();
    r_sl.result = sum_branchless(sorted, ARRAY_LEN);
    r_sl.time_ms = now_ms() - t;
    perf_stop(&r_sl);

    perf_start();
    t = now_ms();
    r_rb.result = sum_branch(random_d, ARRAY_LEN);
    r_rb.time_ms = now_ms() - t;
    perf_stop(&r_rb);

    perf_start();
    t = now_ms();
    r_rl.result = sum_branchless(random_d, ARRAY_LEN);
    r_rl.time_ms = now_ms() - t;
    perf_stop(&r_rl);

    free(sorted); free(random_d);

    /* verify both produce same result on the same data (sanity check) —
     * this comparison already reads all four .result fields, so unlike
     * tests 1-3 there's no separate anti-DCE sink needed here. */
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

    ReportRow rows[] = {
        { "Sorted + branch",     r_sb },
        { "Sorted + branchless", r_sl },
        { "Random + branch",     r_rb },
        { "Random + branchless", r_rl },
    };
    report_table(report, "Test 4 — Branch vs Branchless",
                 "Same sum, computed via conditional jump vs. arithmetic mask, on sorted and random data.",
                 rows, 4);
    if (report) {
        fprintf(report, "**Branch penalty on random data:** %.2f× vs sorted-branch\n\n",
                r_rb.time_ms / r_sb.time_ms);
        fprintf(report, "**Branchless is consistent:** %.2f× random vs sorted\n\n",
                r_rl.time_ms / r_sl.time_ms);
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * main
 * ───────────────────────────────────────────────────────────────────────── */
/* Same wording as the console banner below, just without the 2-space
 * console indent — this goes into a Markdown code block instead. */
static const char *INTERP_GUIDE =
    "Misprediction penalty = pipeline flush + refill latency.\n"
    "On typical x86 out-of-order CPUs this is ~15-20 cycles.\n"
    "At 50% miss rate on 1M branches/rep: ~8M wasted cycles/rep.\n"
    "At 4 GHz that is ~2 ms overhead per rep — matches Test 1.\n"
    "\n"
    "Tests 1/2: direct branches (conditional jumps in the loop).\n"
    "Test 3:    indirect branches (call via register / BTB);\n"
    "           penalty per miss is often higher than direct.\n"
    "Test 4:    branchless mask trick eliminates branches entirely;\n"
    "           consistent throughput regardless of data order.";

int main(int argc, char **argv)
{
    const char *out_path = NULL;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0)
            && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [-o|--output <file.md>]\n"
                   "  -o, --output <file>  write a formatted Markdown report there\n"
                   "                       (in addition to the normal console output)\n",
                   argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s (try --help)\n", argv[i]);
            return 1;
        }
    }

    FILE *report = NULL;
    if (out_path) {
        report = fopen(out_path, "w");
        if (!report) { perror("fopen"); return 1; }
    }

    perf_init();

    printf("══════════════════════════════════════════════════════════════\n");
    printf("  CPU Branch Prediction Benchmark\n");
    printf("  Array size:      %10u elements\n", ARRAY_LEN);
    printf("  Repetitions:     %10u per trial\n", REPS);
    printf("  Perf counters:   %s\n",
           perf_available() ? "AVAILABLE (hardware branch-miss counts shown)"
                            : "unavailable — showing wall-clock timing only");
    printf("══════════════════════════════════════════════════════════════\n\n");

    if (report) {
        char cpu[192];
        get_cpu_model(cpu, sizeof(cpu));
        time_t now = time(NULL);
        char date[32];
        strftime(date, sizeof(date), "%Y-%m-%d", localtime(&now));

        fprintf(report, "## %s\n\n", cpu);
        fprintf(report, "_%s_\n\n", date);
        fprintf(report, "| Array size | Repetitions | Perf counters |\n");
        fprintf(report, "|---:|---:|---|\n");
        fprintf(report, "| %u elements | %u/trial | %s |\n\n",
                ARRAY_LEN, REPS,
                perf_available() ? "available (hardware counts)" : "unavailable (wall-clock only)");
    }

    run_test1(report);
    run_test2(report);
    run_test3(report);
    run_test4(report);

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

    if (report) {
        fprintf(report, "### Interpretation guide\n\n");
        fprintf(report, "```\n%s\n```\n", INTERP_GUIDE);
        fclose(report);
        printf("\nFormatted report written to %s\n", out_path);
    }

#ifdef HAVE_PERF
    if (pfd_branches >= 0) close(pfd_branches);
    if (pfd_misses   >= 0) close(pfd_misses);
#endif

    return 0;
}
