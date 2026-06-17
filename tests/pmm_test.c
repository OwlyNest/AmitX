/* --- Macros ---*/

/* --- Includes ---*/
#include <mm/pmm.h>
#include <screen/printk.h>
#include <lib/string.h>
#include <tests/pmm_test.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
static int tests_passed = 0;
static int tests_failed = 0;

/* --- Prototypes ---*/
static void test_pass(const char *name);
static void test_fail(const char *name);
static void test_alloc_free(void);
static void test_alloc_many(void);
static void test_double_free(void);
static void test_contiguous(void);
static void test_null_free(void);
static void test_exhaustion(void);
static void test_reserve_unreserve(void);

/* --- Functions ---*/

static void test_pass(const char *name) {
    printk("  [PASS] %s\n", name);
    tests_passed++;
}

static void test_fail(const char *name) {
    printk("  [FAIL] %s\n", name);
    tests_failed++;
}

/* ==========================================================================
 * Test: basic alloc / free
 * ======================================================================= */
static void test_alloc_free(void) {
    printk("[pmm_test] alloc/free...\n");

    uint32_t free_before = pmm_get_free_frames();

    void *p = pmm_alloc_frame();
    if (!p) {
        test_fail("alloc returned NULL");
        return;
    }

    if (pmm_get_free_frames() != free_before - 1) {
        test_fail("free count didn't decrease");
        pmm_free_frame(p);
        return;
    }

    pmm_free_frame(p);

    if (pmm_get_free_frames() != free_before) {
        test_fail("free count didn't restore");
        return;
    }

    test_pass("alloc/free");
}

/* ==========================================================================
 * Test: allocate many frames, verify uniqueness
 * ======================================================================= */
static void test_alloc_many(void) {
    printk("[pmm_test] alloc many...\n");

    void *frames[64];
    int i;

    for (i = 0; i < 64; i++) {
        frames[i] = pmm_alloc_frame();
        if (!frames[i]) {
            test_fail("alloc_many ran out of memory early");
            goto cleanup;
        }
    }

    /* Check uniqueness */
    for (i = 0; i < 64; i++) {
        for (int j = i + 1; j < 64; j++) {
            if (frames[i] == frames[j]) {
                test_fail("duplicate frame allocated");
                goto cleanup;
            }
        }
    }

    test_pass("alloc many (64 frames, unique)");

cleanup:
    for (int k = 0; k < i; k++) {
        if (frames[k])
            pmm_free_frame(frames[k]);
    }
}

/* ==========================================================================
 * Test: double free should not corrupt
 * ======================================================================= */
static void test_double_free(void) {
    printk("[pmm_test] double free...\n");

    uint32_t free_before = pmm_get_free_frames();

    void *p = pmm_alloc_frame();
    if (!p) {
        test_fail("alloc failed");
        return;
    }

    pmm_free_frame(p);
    pmm_free_frame(p); /* Double free */

    /* Should still have correct count */
    if (pmm_get_free_frames() != free_before) {
        test_fail("double free corrupted free count");
        return;
    }

    test_pass("double free (no corruption)");
}

/* ==========================================================================
 * Test: contiguous allocation
 * ======================================================================= */
 static void test_contiguous(void) {
    printk("[pmm_test] contiguous alloc...\n");

    void *p = pmm_alloc_frames(4);
    if (!p) {
        test_fail("contiguous alloc returned NULL");
        return;
    }

    /* Verify frame-aligned */
    if ((uint32_t)p & FRAME_SIZE_MASK) {
        test_fail("contiguous alloc not frame-aligned");
        pmm_free_frames(p, 4);
        return;
    }

    /* Verify by allocating next single frame — should be 4 frames after */
    void *q = pmm_alloc_frame();
    if (!q) {
        test_fail("follow-up alloc failed");
        pmm_free_frames(p, 4);
        return;
    }

    uint32_t p_frame = (uint32_t)p >> FRAME_SIZE_SHIFT;
    uint32_t q_frame = (uint32_t)q >> FRAME_SIZE_SHIFT;

    pmm_free_frame(q);
    pmm_free_frames(p, 4);

    if (q_frame == p_frame + 4)
        test_pass("contiguous alloc (4 frames)");
    else
        test_fail("contiguous alloc not actually contiguous");
}

/* ==========================================================================
 * Test: free NULL should not crash
 * ======================================================================= */
static void test_null_free(void) {
    printk("[pmm_test] NULL free...\n");

    uint32_t free_before = pmm_get_free_frames();
    pmm_free_frame(NULL);

    if (pmm_get_free_frames() != free_before) {
        test_fail("NULL free changed free count");
        return;
    }

    test_pass("NULL free (no crash, no change)");
}

/* ==========================================================================
 * Test: alloc a large batch, then free all
 * ======================================================================= */
static void test_exhaustion(void) {
    printk("[pmm_test] exhaustion...\n");

    uint32_t free_before = pmm_get_free_frames();
    /* Use a static array to avoid large stack frames */
    static void *frames[512];
    int count = 0;

    /* Don't exhaust everything, just test a large batch */
    int target = (free_before > 512) ? 512 : (int)(free_before / 2);

    for (count = 0; count < target; count++) {
        frames[count] = pmm_alloc_frame();
        if (!frames[count])
            break;
    }

    if (count == 0 && target > 0) {
        test_fail("couldn't alloc any frames");
        return;
    }

    for (int i = 0; i < count; i++) {
        pmm_free_frame(frames[i]);
    }

    if (pmm_get_free_frames() != free_before) {
        test_fail("free count mismatch after bulk alloc/free");
        return;
    }

    test_pass("exhaustion test (bulk alloc/free)");
}

/* ==========================================================================
 * Test: reserve and unreserve regions
 * ======================================================================= */
static void test_reserve_unreserve(void) {
    printk("[pmm_test] reserve/unreserve...\n");

    uint32_t free_before = pmm_get_free_frames();

    /* Reserve a region we know is free (pick something high, above bitmap) */
    uintptr_t test_addr = 0x00200000; /* 2MB */
    pmm_reserve_region(test_addr, FRAME_SIZE * 4);

    if (pmm_get_free_frames() != free_before - 4) {
        test_fail("reserve didn't consume 4 frames");
        pmm_unreserve_region(test_addr, FRAME_SIZE * 4);
        return;
    }

    pmm_unreserve_region(test_addr, FRAME_SIZE * 4);

    if (pmm_get_free_frames() != free_before) {
        test_fail("unreserve didn't restore free count");
        return;
    }

    test_pass("reserve/unreserve");
}

/* ==========================================================================
 * Test: aligned frames
 * ======================================================================= */
void test_pmm_aligned(void) {
    printk("[pmm_test] aligned alloc...\n");

    void *p = pmm_alloc_aligned(1, 4);
    if (!p) {
        test_fail("aligned alloc failed");
        return;
    }

    uint32_t frame = (uint32_t)p >> FRAME_SIZE_SHIFT;
    if (frame & 3) {
        test_fail("frame ot aligned to 4");
    }

    test_pass("frame aligned to 4");
    pmm_free_frames(p, 1);
}
/* ==========================================================================
 * Run all PMM tests
 * ======================================================================= */
void pmm_run_tests(void) {
	tests_failed = 0;
	tests_passed = 0;
    printk("\n========== PMM Test Suite ==========\n");

    test_alloc_free();
    test_alloc_many();
    test_double_free();
    test_contiguous();
    test_null_free();
    test_exhaustion();
    test_reserve_unreserve();
    test_pmm_aligned();

    printk("====================================\n");
    printk("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printk("====================================\n\n");
}