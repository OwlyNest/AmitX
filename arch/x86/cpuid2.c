/*
	* arch/x86/cpuid2.c - [Enter description]
	* Author:   amity
	* Date:     Sat Jul 11 01:31:05 2026
	* Copyright © 2026 OwlyNest
*/

/* --- Styling Instructions ---
	* Encoding:                      UTF-8, Unix line endings
	* Text font:                     Monospace
	* Line width:                    Max 80 characters
	* Indentation:                   Use 4 spaces
	* Brace style:                   Same line as control statement
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/
// From Google I guess, someone copied it on stackoverflow
#define ARRAY_SIZE(a)                               \
  ((sizeof(a) / sizeof(*(a))) /                     \
  (size_t)(!(sizeof(a) % sizeof(*(a)))))
/* --- Includes ---*/
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <arch/x86/cpuid.h>
#include <screen/printk.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
cpu_info_t info = { 0 };
cpuid_raw_db_t db = { 0 };

static const uint32_t simple_basic[] = {
    0x00000001,
    0x00000002,
    0x00000003,

    0x00000005,
    0x00000006,
    // 
    0x00000015,
    0x00000016
};

static const uint32_t simple_extended[] = {
    0x80000001,
    0x80000002,
    0x80000003,
    0x80000004,
    0x80000005,
    0x80000006,
    0x80000007,
    0x80000008,
    0x8000000A,
    0x80000019,
    0x8000001A,
    0x8000001B,
    0x8000001C,
    //✨Special✨
    0x8000001E,
    0x8000001F,
    0x80000020,
    0x80000021,
};
/* --- Prototypes ---*/
static inline void cpuid_exec(cpuid_raw_db_t *db, uint32_t leaf, uint32_t subleaf);
static inline void u32_to_bytes(uint32_t val, char *buf);

/* --- Functions ---*/

/* ==========================================================================
 *                                                                          *
 * cpuid_available()                                                        *
 *                                                                          *
 * Check if the CPUID instruction is supported                              *
 * CPUID is supported if we can flip bit 21 (the ID bit) in EFLAGS          *
 * Returns non-zero if CPUID is available, zero otherwise                   *
 *                                                                          *
 ========================================================================== */
int cpuid_available(void) {
#ifdef __x86_64__
    uint64_t orig, mod;

    __asm__ __volatile__ (
        "pushfq\n\t"                    // Save RFLAGS
        "popq %0\n\t"                   // orig = RFLAGS
        "movq %0, %1\n\t"               // mod = orig
        "xorq $0x00200000, %1\n\t"      // Flip ID bit (bit 21)
        "pushq %1\n\t"                  // Push modified flags
        "popfq\n\t"                     // Load modified RFLAGS
        "pushfq\n\t"                    // Store RFLAGS again
        "popq %1\n\t"                   // mod = new RFLAGS
        "pushq %0\n\t"                  // Push original flags
        "popfq\n\t"                     // Restore original RFLAGS
        "xorq %0, %1"                   // mod = changed bits
        : "=r" (orig), "=r" (mod)
        :
        : "cc", "memory"
    );

    return (mod & 0x00200000) != 0;
#else
    uint32_t orig, mod;

    __asm__ __volatile__ (
        "pushfl\n\t"                    // Save EFLAGS
        "popl %0\n\t"                   // orig = EFLAGS
        "movl %0, %1\n\t"               // mod = orig
        "xorl $0x00200000, %1\n\t"      // Flip ID bit (bit 21)
        "pushl %1\n\t"                  // Push modified flags
        "popfl\n\t"                     // Load modified EFLAGS
        "pushfl\n\t"                    // Store EFLAGS again
        "popl %1\n\t"                   // mod = new EFLAGS
        "pushl %0\n\t"                  // Push original flags
        "popfl\n\t"                     // Restore original EFLAGS
        "xorl %0, %1"                   // mod = changed bits
        : "=r" (orig), "=r" (mod)
        :
        : "cc", "memory"
    );

    return (mod & 0x00200000) != 0;
#endif
}

/* ==========================================================================
 *                                                                          *
 * cpuid_raw_find()                                                         *
 *                                                                          *
 * Find raw CPUID data by leaf and subleaf                                  *
 *                                                                          *
 ========================================================================== */
static const cpuid_raw_t *cpuid_raw_find(const cpuid_raw_db_t *db, uint32_t leaf, uint32_t subleaf) {
	for (uint32_t i = 0; i < db->count; i++) {
		cpuid_raw_t raw = db->entries[i];
		if (raw.leaf == leaf && raw.subleaf == subleaf) {
			return &db->entries[i]; /* return raw; makes clangd upset */
		}
	}
	return NULL;
}

/* ==========================================================================
 *                                                                          *
 * cpuid_exec()                                                             *
 *                                                                          *
 * Execute CPUID with EAX=leaf, ECX=subleaf and return all four registers   *
 *                                                                          *
 ========================================================================== */
static inline void cpuid_exec(cpuid_raw_db_t *db, uint32_t leaf, uint32_t subleaf) {
	uint32_t in_subleaf = subleaf;
#ifdef __x86_64__
        uint32_t ra, rb, rc, rd;
		rc = subleaf;
    
        __asm__ __volatile__ (
            "pushq %%rbx\n\t"
            "cpuid\n\t"
            "movl %%ebx, %1\n\t"
            "popq %%rbx"
            : "=a"(ra), "=r"(rb), "+c"(rc), "=d"(rd)
            : "a"(leaf)
            : "cc", "memory"
        );
    
		db->entries[db->count++] = (cpuid_raw_t) {
			.leaf = leaf,
			.subleaf = in_subleaf,
			.eax = ra,
			.ebx = rb,
			.ecx = rc,
			.edx = rd
		};
#else
        uint32_t ra, rb, rc, rd;
		rc = subleaf;
    
        __asm__ __volatile__ (
            "pushl %%ebx\n\t"
            "cpuid\n\t"
            "movl %%ebx, %1\n\t"
            "popl %%ebx"
            : "=a"(ra), "=r"(rb), "+c"(rc), "=d"(rd) // '+' makes subleaf read/write
            : "a"(leaf)
            : "cc", "memory"
        );
    
        db->entries[db->count++] = (cpuid_raw_t) {
			.leaf = leaf,
			.subleaf = in_subleaf,
			.eax = ra,
			.ebx = rb,
			.ecx = rc,
			.edx = rd
		};
#endif
}

/* ==========================================================================
 *                                                                          *
 * u32_to_bytes()                                                           *
 *                                                                          *
 * Convert a little-endian uint32_t to 4 bytes                              *
 *                                                                          *
 ========================================================================== */
static inline void u32_to_bytes(uint32_t val, char *buf) {
    buf[0] = (char)(val & 0xFF);
    buf[1] = (char)((val >> 8)  & 0xFF);
    buf[2] = (char)((val >> 16) & 0xFF);
    buf[3] = (char)((val >> 24) & 0xFF);
}

/* ==========================================================================
 *                                                                          *
 * cpuid_raw_pass()                                                         *
 *                                                                          *
 * Retrieve raw CPUID data                                                  *
 *                                                                          *
 ========================================================================== */
void cpuid_raw_pass(void) {
	cpuid_exec(&db, 0x00000000, 0);
	uint32_t max_basic = cpuid_raw_find(&db, 0, 0)->eax;
    for (size_t i = 0; i < ARRAY_SIZE(simple_basic); i++) {
        if (max_basic >= simple_basic[i]) {
            cpuid_exec(&db, simple_basic[i], 0);
        }
    }
    for (uint32_t idx = 0;; idx++) {
        cpuid_exec(&db, 4, idx);
    
        if ((cpuid_raw_find(&db, 4, idx)->eax & 0x1F) == 0) {
            break;
        }
    }

    cpuid_exec(&db, 0x00000007, 0);
    for (uint32_t idx = 1; idx <= cpuid_raw_find(&db, 7, 0)->eax; idx++) {
        cpuid_exec(&db, 7, idx);
    }

    for (uint32_t idx = 0;; idx++) {
        cpuid_exec(&db, 0x0B, idx);
    
        uint32_t ecx = cpuid_raw_find(&db, 0x0B, idx)->ecx;
        uint32_t level_type = (ecx >> 8) & 0xFF;
    
        if (level_type == 0) {
            break;
        }
    }

	cpuid_exec(&db, 0x0D, 0);
    cpuid_exec(&db, 0x0D, 1);
    const cpuid_raw_t *leaf = cpuid_raw_find(&db, 0x0D, 0);
    uint64_t bitmap = ((uint64_t)leaf->edx << 32) | leaf->eax;
    for (uint32_t i = 2; i < 64; i++) {
        if (bitmap & (1ULL << i)) {
            cpuid_exec(&db, 0x0D, i);
        }
    }

    
	cpuid_exec(&db, 0x80000000, 0);
	uint32_t max_extended = cpuid_raw_find(&db, 0x80000000, 0)->eax;
    for (size_t i = 0; i < ARRAY_SIZE(simple_extended); i++) {
        if (max_extended >= simple_extended[i]) {
            cpuid_exec(&db, simple_extended[i], 0);
        }
    }

    if (max_extended >= 0x8000001D) {
        for (uint32_t idx = 0;; idx++) {
            cpuid_exec(&db, 0x8000001D, idx);
        
            if ((cpuid_raw_find(&db, 0x8000001D, idx)->eax & 0x1F) == 0) {
                break;
            }
        }
    }
}

/* ==========================================================================
 *                                                                          *
 * cpuid_dump_db()                                                          *
 *                                                                          *
 * Dump raw database in a formatted table                                   *
 *                                                                          *
 ========================================================================== */
void cpuid_dump_db(void) {
    /*
     * Don't order just dump
     * User can read, I think, probably
    */
    printk("Leaf       Sub EAX        EBX        ECX        EDX\n");
    for (uint32_t idx = 0; idx < db.count; idx++) {
        const cpuid_raw_t *leaf = &db.entries[idx];
        printk("%#010x %-3u %#010x %#010x %#010x %#010x\n",
            leaf->leaf,
            leaf->subleaf,
            leaf->eax,
            leaf->ebx,
            leaf->ecx,
            leaf->edx);
        
    }
}


static void cpuid_decode_vendor(cpuid_raw_db_t *db, cpu_info_t *info) {
    const cpuid_raw_t *leaf = cpuid_raw_find(db, 0, 0);

    u32_to_bytes(leaf->ebx, &info->vendor[0]);
    u32_to_bytes(leaf->edx, &info->vendor[4]);
    u32_to_bytes(leaf->ecx, &info->vendor[8]);
    info->vendor[12] = '\0';
}

static void cpuid_decode_brand(cpuid_raw_db_t *db, cpu_info_t *info) {

    info->brand[0] = '\0';

    const cpuid_raw_t *leaf = cpuid_raw_find(db, 0x80000000, 0);


    if (leaf->eax < 0x80000004) {
        return;                         // Brand string not supported
    }

    for (int i = 0; i < 3; i++) {
        const cpuid_raw_t *brand_leaf = cpuid_raw_find(db, 0x80000002 + i, 0);

        u32_to_bytes(brand_leaf->eax, &info->brand[i * 16 + 0]);
        u32_to_bytes(brand_leaf->ebx, &info->brand[i * 16 + 4]);
        u32_to_bytes(brand_leaf->ecx, &info->brand[i * 16 + 8]);
        u32_to_bytes(brand_leaf->edx, &info->brand[i * 16 + 12]);
    }
    info->brand[48] = '\0';
}

static void cpuid_decode_psn(cpuid_raw_db_t *db, cpu_info_t *info) {
    const cpuid_raw_t *leaf1 = cpuid_raw_find(db, 1, 0);
    const cpuid_raw_t *leaf3 = cpuid_raw_find(db, 3, 0);
    info->psn[0] = leaf1->eax;
    info->psn[1] = leaf3->ecx;
    info->psn[2] = leaf3->edx;
}

static void cpuid_decode_proc(cpuid_raw_db_t *db, cpuid_proc_info_t *info) {
    const cpuid_raw_t *leaf1 = cpuid_raw_find(db, 1, 0);

    info->stepping      = (uint8_t)( leaf1->eax        & 0x0F);
    info->model         = (uint8_t)((leaf1->eax >> 4)  & 0x0F);
    info->family        = (uint8_t)((leaf1->eax >> 8)  & 0x0F);
    info->proc_type     = (uint8_t)((leaf1->eax >> 12) & 0x03);
    info->ext_model     = (uint8_t)((leaf1->eax >> 16) & 0x0F);
    info->ext_family    = (uint8_t)((leaf1->eax >> 20) & 0xFF);

    info->brand_index       = (uint8_t)(leaf1->ebx & 0xFF);
    info->clflush_line_size = (uint8_t)(((leaf1->ebx >> 8) & 0xFF) * 8);
    info->max_logical_ids   = (uint8_t)((leaf1->ebx >> 16) & 0xFF);
    info->initial_apic_id   = (leaf1->ebx >> 24) & 0xFF;

    // Intel/AMD display rules: if family == 6 or 15, display model
    // is (ext_model << 4) + model. If family == 15, display family
    // is ext_family + family.
    uint32_t family = info->family;
    uint32_t model  = info->model;

    if (family == 0x06 || family == 0x0F) {
        model = (info->ext_model << 4) | info->model;
    }
    if (family == 0x0F) {
        family = info->ext_family + info->family;
    }

    info->display_model  = (uint8_t)model;
    info->display_family = (uint8_t)family;
}

static void cpuid_decode_feat(cpuid_raw_db_t *db, cpuid_feat_t *info) {

    const cpuid_raw_t *leaf = cpuid_raw_find(db, 1, 0);

    uint32_t ecx = leaf->ecx;
    uint32_t edx = leaf->edx;

    info->fpu = (uint8_t)((edx >> 0) & 0x01);
    info->vme = (uint8_t)((edx >> 1) & 0x01);
    info->de = (uint8_t)((edx >> 2) & 0x01);
    info->pse = (uint8_t)((edx >> 3) & 0x01);
    info->tsc = (uint8_t)((edx >> 4) & 0x01);
    info->msr = (uint8_t)((edx >> 5) & 0x01);
    info->pae = (uint8_t)((edx >> 6) & 0x01);
    info->mce = (uint8_t)((edx >> 7) & 0x01);
    info->cx8 = (uint8_t)((edx >> 8) & 0x01);
    info->apic = (uint8_t)((edx >> 9) & 0x01);
    
    info->sep = (uint8_t)((edx >> 11) & 0x01);
    info->mtrr = (uint8_t)((edx >> 12) & 0x01);
    info->pge = (uint8_t)((edx >> 13) & 0x01);
    info->mca = (uint8_t)((edx >> 14) & 0x01);
    info->cmov = (uint8_t)((edx >> 15) & 0x01);
    info->pat = (uint8_t)((edx >> 16) & 0x01);
    info->pse36 = (uint8_t)((edx >> 17) & 0x01);
    info->psn = (uint8_t)((edx >> 18) & 0x01);
    info->clfsh = (uint8_t)((edx >> 19) & 0x01);
    info->nx = (uint8_t)((edx >> 20) & 0x01);
    info->ds = (uint8_t)((edx >> 21) & 0x01);
    info->acpi = (uint8_t)((edx >> 22) & 0x01);
    info->mmx = (uint8_t)((edx >> 23) & 0x01);
    info->fxsr = (uint8_t)((edx >> 24) & 0x01);
    info->sse = (uint8_t)((edx >> 25) & 0x01);
    info->sse2 = (uint8_t)((edx >> 26) & 0x01);
    info->ss = (uint8_t)((edx >> 27) & 0x01);
    info->htt = (uint8_t)((edx >> 28) & 0x01);
    info->tm = (uint8_t)((edx >> 29) & 0x01);
    info->ia64 = (uint8_t)((edx >> 30) & 0x01);
    info->pbe = (uint8_t)((edx >> 31) & 0x01);

    info->sse3 = (uint8_t)((ecx >> 0) & 0x01);
    info->pclmulqdq = (uint8_t)((ecx >> 1) & 0x01);
    info->dtes64 = (uint8_t)((ecx >> 2) & 0x01);
    info->monitor = (uint8_t)((ecx >> 3) & 0x01);
    info->ds_cpl = (uint8_t)((ecx >> 4) & 0x01);
    info->vmx = (uint8_t)((ecx >> 5) & 0x01);
    info->smx = (uint8_t)((ecx >> 6) & 0x01);
    info->est = (uint8_t)((ecx >> 7) & 0x01);
    info->tm2 = (uint8_t)((ecx >> 8) & 0x01);
    info->ssse3 = (uint8_t)((ecx >> 9) & 0x01);
    info->cntx_id = (uint8_t)((ecx >> 10) & 0x01);
    info->sdbg = (uint8_t)((ecx >> 11) & 0x01);
    info->fma = (uint8_t)((ecx >> 12) & 0x01);
    info->cx16 = (uint8_t)((ecx >> 13) & 0x01);
    info->xtpr = (uint8_t)((ecx >> 14) & 0x01);
    info->pdcm = (uint8_t)((ecx >> 15) & 0x01);
    // reserved
    info->pcid = (uint8_t)((ecx >> 17) & 0x01);
    info->dca = (uint8_t)((ecx >> 18) & 0x01);
    info->sse4_1 = (uint8_t)((ecx >> 19) & 0x01);
    info->sse4_2 = (uint8_t)((ecx >> 20) & 0x01);
    info->x2apic = (uint8_t)((ecx >> 21) & 0x01);
    info->movbe = (uint8_t)((ecx >> 22) & 0x01);
    info->popcnt = (uint8_t)((ecx >> 23) & 0x01);
    info->tsc_deadline = (uint8_t)((ecx >> 24) & 0x01);
    info->aes_ni = (uint8_t)((ecx >> 25) & 0x01);
    info->xsave = (uint8_t)((ecx >> 26) & 0x01);
    info->osxsave = (uint8_t)((ecx >> 27) & 0x01);
    info->avx = (uint8_t)((ecx >> 28) & 0x01);
    info->f16c = (uint8_t)((ecx >> 29) & 0x01);
    info->rdrnd = (uint8_t)((ecx >> 30) & 0x01);
    info->hypervisor = (uint8_t)((ecx >> 31) & 0x01);
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_ext_feat()                                                     *
 *                                                                          *
 * Decode Extended features from leaf 0x80000001                            *
 *                                                                          *
 ========================================================================== */
static void cpuid_decode_ext_feat(cpuid_raw_db_t *db, cpuid_ext_feat_t *info) {
    uint32_t ecx, edx;

    const cpuid_raw_t *leaf = cpuid_raw_find(db, 0x80000001, 0);
    ecx = leaf->ecx;
    edx = leaf->edx;

    info->fpu = (uint8_t)((edx >> 0) & 0x01);
    info->vme = (uint8_t)((edx >> 1) & 0x01);
    info->de = (uint8_t)((edx >> 2) & 0x01);
    info->pse = (uint8_t)((edx >> 3) & 0x01);
    info->tsc = (uint8_t)((edx >> 4) & 0x01);
    info->msr = (uint8_t)((edx >> 5) & 0x01);
    info->pae = (uint8_t)((edx >> 6) & 0x01);
    info->mce = (uint8_t)((edx >> 7) & 0x01);
    info->cx8 = (uint8_t)((edx >> 8) & 0x01);
    info->apic = (uint8_t)((edx >> 9) & 0x01);
    info->syscall_k6 = (uint8_t)((edx >> 10) & 0x01);
    info->syscall = (uint8_t)((edx >> 11) & 0x01);
    info->mtrr = (uint8_t)((edx >> 12) & 0x01);
    info->pge = (uint8_t)((edx >> 13) & 0x01);
    info->mca = (uint8_t)((edx >> 14) & 0x01);
    info->cmov = (uint8_t)((edx >> 15) & 0x01);
    info->pat = (uint8_t)((edx >> 16) & 0x01);
    info->pse36 = (uint8_t)((edx >> 17) & 0x01);
    info->ecc_k7 = (uint8_t)((edx >> 18) & 0x01);
    info->ecc = (uint8_t)((edx >> 19) & 0x01);
    info->nx = (uint8_t)((edx >> 20) & 0x01);
    info->sem = (uint8_t)((edx >> 21) & 0x01);
    info->mmxext = (uint8_t)((edx >> 22) & 0x01);
    info->mmx = (uint8_t)((edx >> 23) & 0x01);
    info->fxsr = (uint8_t)((edx >> 24) & 0x01);
    info->fxsr_opt = (uint8_t)((edx >> 25) & 0x01);
    info->pdpe1gb = (uint8_t)((edx >> 26) & 0x01);
    info->rdtscp = (uint8_t)((edx >> 27) & 0x01);
    info->rex32_k8 = (uint8_t)((edx >> 28) & 0x01);
    info->lm = (uint8_t)((edx >> 29) & 0x01);
    info->tdnowext = (uint8_t)((edx >> 30) & 0x01);
    info->tdnow = (uint8_t)((edx >> 31) & 0x01);

    info->lahf_lm = (uint8_t)((ecx >> 0) & 0x01);
    info->cmp_legacy = (uint8_t)((ecx >> 1) & 0x01);
    info->svm = (uint8_t)((ecx >> 2) & 0x01);
    info->extapic = (uint8_t)((ecx >> 3) & 0x01);
    info->cr8_legacy = (uint8_t)((ecx >> 4) & 0x01);
    info->abm = (uint8_t)((ecx >> 5) & 0x01);
    info->sse4a = (uint8_t)((ecx >> 6) & 0x01);
    info->misalignsse = (uint8_t)((ecx >> 7) & 0x01);
    info->tdnowprefetch = (uint8_t)((ecx >> 8) & 0x01);
    info->osvw = (uint8_t)((ecx >> 9) & 0x01);
    info->ibs = (uint8_t)((ecx >> 10) & 0x01);
    info->xop = (uint8_t)((ecx >> 11) & 0x01);
    info->skinit = (uint8_t)((ecx >> 12) & 0x01);
    info->wdt = (uint8_t)((ecx >> 13) & 0x01);
    info->tbm0 = (uint8_t)((ecx >> 14) & 0x01);
    info->lwp = (uint8_t)((ecx >> 15) & 0x01);
    info->fma4 = (uint8_t)((ecx >> 16) & 0x01);
    info->tce = (uint8_t)((ecx >> 17) & 0x01);
    info->cvt16 = (uint8_t)((ecx >> 18) & 0x01);
    info->nodeid_msr = (uint8_t)((ecx >> 19) & 0x01);
    // reserved
    info->tbm = (uint8_t)((ecx >> 21) & 0x01);
    info->topoext = (uint8_t)((ecx >> 22) & 0x01);
    info->perfctr_core = (uint8_t)((ecx >> 23) & 0x01);
    info->perfctr_nb = (uint8_t)((ecx >> 24) & 0x01);
    info->StreamPerfMon = (uint8_t)((ecx >> 25) & 0x01);
    info->dbx = (uint8_t)((ecx >> 26) & 0x01);
    info->perftsc = (uint8_t)((ecx >> 27) & 0x01);
    info->pcx_l2i_l3 = (uint8_t)((ecx >> 28) & 0x01);
    info->monitorx = (uint8_t)((ecx >> 29) & 0x01);
    info->addr_mask_ext = (uint8_t)((ecx >> 30) & 0x01);
    //reserved
}

static void cpuid_decode_feat7(cpuid_raw_db_t *db, cpuid_feat7_t *info) {
    uint32_t eax, ebx, ecx, edx;
    /* --- Sub Leaf 0 --- */
    const cpuid_raw_t *leaf70 = cpuid_raw_find(db, 7, 0);
    ebx = leaf70->ebx;
    ecx = leaf70->ecx;
    edx = leaf70->edx;
    // EBX
    info->fsgsbase = (uint8_t)((ebx >> 0) & 0x01);
    info->tsc_adjust = (uint8_t)((ebx >> 1) & 0x01);
    info->sgx = (uint8_t)((ebx >> 2) & 0x01);
    info->bmi1 = (uint8_t)((ebx >> 3) & 0x01);
    info->hle = (uint8_t)((ebx >> 4) & 0x01);
    info->avx2 = (uint8_t)((ebx >> 5) & 0x01);
    info->fdp_excptn_only = (uint8_t)((ebx >> 6) & 0x01);
    info->smep = (uint8_t)((ebx >> 7) & 0x01);
    info->bmi2 = (uint8_t)((ebx >> 8) & 0x01);
    info->erms = (uint8_t)((ebx >> 9) & 0x01);
    info->invpcid = (uint8_t)((ebx >> 10) & 0x01);
    info->rtm = (uint8_t)((ebx >> 11) & 0x01);
    info->rdt_m_pqm = (uint8_t)((ebx >> 12) & 0x01);
    info->fcs_fds_deprecation = (uint8_t)((ebx >> 13) & 0x01);
    info->mpx = (uint8_t)((ebx >> 14) & 0x01);
    info->rdt_a_pqe = (uint8_t)((ebx >> 15) & 0x01);
    info->avx512_f = (uint8_t)((ebx >> 16) & 0x01);
    info->avx512_dq = (uint8_t)((ebx >> 17) & 0x01);
    info->rdseed = (uint8_t)((ebx >> 18) & 0x01);
    info->adx = (uint8_t)((ebx >> 19) & 0x01);
    info->smap = (uint8_t)((ebx >> 20) & 0x01);
    info->avx512_ifma = (uint8_t)((ebx >> 21) & 0x01);
    info->pmcommit = (uint8_t)((ebx >> 22) & 0x01);
    info->clflushopt = (uint8_t)((ebx >> 23) & 0x01);
    info->clwb = (uint8_t)((ebx >> 24) & 0x01);
    info->pt = (uint8_t)((ebx >> 25) & 0x01);
    info->avx512_pf = (uint8_t)((ebx >> 26) & 0x01);
    info->avx512_er = (uint8_t)((ebx >> 27) & 0x01);
    info->avx512_cd = (uint8_t)((ebx >> 28) & 0x01);
    info->sha = (uint8_t)((ebx >> 29) & 0x01);
    info-> avx512_bw = (uint8_t)((ebx >> 30) & 0x01);
    info->avx512_vl = (uint8_t)((ebx >> 31) & 0x01);
    // ECX
    info->prefetchwt1 = (uint8_t)((ecx >> 0) & 0x01);
    info->avx512_vbmi = (uint8_t)((ecx >> 1) & 0x01);
    info->umip = (uint8_t)((ecx >> 2) & 0x01);
    info->pku = (uint8_t)((ecx >> 3) & 0x01);
    info->ospke = (uint8_t)((ecx >> 4) & 0x01);
    info->waitpkg = (uint8_t)((ecx >> 5) & 0x01);
    info->avx512_vmbi2 = (uint8_t)((ecx >> 6) & 0x01);
    info->cet_ss = (uint8_t)((ecx >> 7) & 0x01);
    info->gfni = (uint8_t)((ecx >> 8) & 0x01);
    info->vaes = (uint8_t)((ecx >> 9) & 0x01);
    info->vpclmulqdq = (uint8_t)((ecx >> 10) & 0x01);
    info->avx512_vnni = (uint8_t)((ecx >> 11) & 0x01);
    info->avx512_bitalg = (uint8_t)((ecx >> 12) & 0x01);
    info->tme_en = (uint8_t)((ecx >> 13) & 0x01);
    info->avx512_vpopcntdq = (uint8_t)((ecx >> 14) & 0x01);
    info->fzm = (uint8_t)((ecx >> 15) & 0x01);
    info->la57 = (uint8_t)((ecx >> 16) & 0x01);
    info->mawau = (uint32_t)((ecx >> 17) & 0x1F);
    info->rdpid = (uint8_t)((ecx >> 22) & 0x01);
    info->kl = (uint8_t)((ecx >> 23) & 0x01);
    info->bus_lock_detect = (uint8_t)((ecx >> 24) & 0x01);
    info->cldemote = (uint8_t)((ecx >> 25) & 0x01);
    info->mprr = (uint8_t)((ecx >> 26) & 0x01);
    info->movdiri = (uint8_t)((ecx >> 27) & 0x01);
    info->movdir64b = (uint8_t)((ecx >> 28) & 0x01);
    info->enqcmd = (uint8_t)((ecx >> 29) & 0x01);
    info->sgx_lc = (uint8_t)((ecx >> 30) & 0x01);
    info->pks4 = (uint8_t)((ecx >> 31) & 0x01);
    // EDX
    info->sgx_term = (uint8_t)((edx >> 0) & 0x01);
    info->sgx_keys = (uint8_t)((edx >> 1) & 0x01);
    info->avx512_4vnniw = (uint8_t)((edx >> 2) & 0x01);
    info->avx512_4fmaps = (uint8_t)((edx >> 3) & 0x01);
    info->fsrm = (uint8_t)((edx >> 4) & 0x01);
    info->uintr = (uint8_t)((edx >> 5) & 0x01);
    // reserved
    // reserved
    info->avx512_vp2intersect = (uint8_t)((edx >> 8) & 0x01);
    info->srbds_ctrl = (uint8_t)((edx >> 9) & 0x01);
    info->md_clear = (uint8_t)((edx >> 10) & 0x01);
    info->rtm_always_abort = (uint8_t)((edx >> 11) & 0x01);
    // reserved
    info->rtm_force_abort = (uint8_t)((edx >> 13) & 0x01);
    info->serialize = (uint8_t)((edx >> 14) & 0x01);
    info->hybrid = (uint8_t)((edx >> 15) & 0x01);
    info->tsxldtrk = (uint8_t)((edx >> 16) & 0x01);
    // reserved
    info->pconfig = (uint8_t)((edx >> 18) & 0x01);
    info->lbr = (uint8_t)((edx >> 19) & 0x01);
    info->cet_ibt = (uint8_t)((edx >> 20) & 0x01);
    // reserved
    info->iamx_bf16 = (uint8_t)((edx >> 22) & 0x01);
    info->avx512_fp16 = (uint8_t)((edx >> 23) & 0x01);
    info->iamx_tile = (uint8_t)((edx >> 24) & 0x01);
    info->iamx_int8 = (uint8_t)((edx >> 25) & 0x01);
    info->spec_ctrl = (uint8_t)((edx >> 26) & 0x01);
    info->stibp = (uint8_t)((edx >> 27) & 0x01);
    info->l1d_flush = (uint8_t)((edx >> 28) & 0x01);
    info->arch_capabilities = (uint8_t)((edx >> 29) & 0x01);
    info->core_capabilities = (uint8_t)((edx >> 30) & 0x01);
    info->ssbd = (uint8_t)((edx >> 31) & 0x01);
    /* --- Sub Leaf 1 --- */
    const cpuid_raw_t *leaf71 = cpuid_raw_find(db, 7, 1);
    if (leaf71 == NULL) {
        return;
    }
    eax = leaf71->eax;
    ebx = leaf71->ebx;
    ecx = leaf71->ecx;
    edx = leaf71->edx;
    // EAX
    info->sha512 = (uint8_t)((eax >> 0) & 0x01);
    info->sm3 = (uint8_t)((eax >> 1) & 0x01);
    info->sm4 = (uint8_t)((eax >> 2) & 0x01);
    info->rao_int = (uint8_t)((eax >> 3) & 0x01);
    info->amx_vnni = (uint8_t)((eax >> 4) & 0x01);
    info->avx512_bf16 = (uint8_t)((eax >> 5) & 0x01);
    info->lass = (uint8_t)((eax >> 6) & 0x01);
    info->cmpccxadd = (uint8_t)((eax >> 7) & 0x01);
    info->archperfmonext = (uint8_t)((eax >> 8) & 0x01);
    info->dedup = (uint8_t)((eax >> 9) & 0x01);
    info->fzrm = (uint8_t)((eax >> 10) & 0x01);
    info->fsrs = (uint8_t)((eax >> 11) & 0x01);
    info->rsrcs = (uint8_t)((eax >> 12) & 0x01);
    // reserved
    // reserved
    // reserved
    // reserved
    info->fred = (uint8_t)((eax >> 17) & 0x01);
    info->lkgs = (uint8_t)((eax >> 18) & 0x01);
    info->wrmsrns = (uint8_t)((eax >> 19) & 0x01);
    info->nmi_src = (uint8_t)((eax >> 20) & 0x01);
    info->iamx_fp16 = (uint8_t)((eax >> 21) & 0x01);
    info->hreset = (uint8_t)((eax >> 22) & 0x01);
    info->avx_ifma = (uint8_t)((eax >> 23) & 0x01);
    // reserved
    // reserved
    info->lam = (uint8_t)((eax >> 26) & 0x01);
    info->msrlist = (uint8_t)((eax >> 27) & 0x01);
    // reserved
    // reserved
    info->invd_disable_post_bios_done = (uint8_t)((eax >> 30) & 0x01);
    info->movrs = (uint8_t)((eax >> 31) & 0x01);
    // EBX
    info->ppin = (uint8_t)((ebx >> 0) & 0x01);
    info->pbndkb = (uint8_t)((ebx >> 1) & 0x01);
    // reserved
    info->cpuid_maxval_lim_rmv = (uint8_t)((ebx >> 3) & 0x01);
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    info->mpsadbw_512 = (uint8_t)((ebx >> 28) & 0x01);
    // reserved
    info->avx512_rao_fp = (uint8_t)((ebx >> 30) & 0x01);
    // ECX
    info->rdt_m_asym = (uint8_t)((ecx >> 0) & 0x01);
    info->rdt_a_asym = (uint8_t)((ecx >> 1) & 0x01);
    info->reduced_isa = (uint8_t)((ecx >> 2) & 0x01);
    // reserved
    info->sipi64 = (uint8_t)((ecx >> 4) & 0x01);
    info->msr_imm = (uint8_t)((ecx >> 5) & 0x01);
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    info->ace = (uint8_t)((ecx >> 11) & 0x01);
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // EDX
    // reserved
    info->avx512_vnni_fp16 = (uint8_t)((edx >> 1) & 0x01);
    info->avx512_vnni_int8 = (uint8_t)((edx >> 2) & 0x01);
    info->avx512_ne_convert = (uint8_t)((edx >> 3) & 0x01);
    info->avx_vnni_int8 = (uint8_t)((edx >> 4) & 0x01);
    info->avx_ne_convert = (uint8_t)((edx >> 5) & 0x01);
    // reserved
    // reserved
    info->iamx_complex = (uint8_t)((edx >> 8) & 0x01);
    // reserved
    info->avx_vnni_int16 = (uint8_t)((edx >> 10) & 0x01);
    info->avx512_vnni_int16 = (uint8_t)((edx >> 11) & 0x01);
    // reserved
    info->utmr = (uint8_t)((edx >> 13) & 0x01);
    info->prefetchi = (uint8_t)((edx >> 14) & 0x01);
    info->user_msr = (uint8_t)((edx >> 15) & 0x01);
    info->avx512_bf16_ne = (uint8_t)((edx >> 16) & 0x01);
    info->uiret_uif_from_rflags = (uint8_t)((edx >> 17) & 0x01);
    info->cet_sss = (uint8_t)((edx >> 18) & 0x01);
    info->avx10 = (uint8_t)((edx >> 19) & 0x01);
    // reserved
    info->apx_f = (uint8_t)((edx >> 21) & 0x01);
    info->sec_tee_attestation = (uint8_t)((edx >> 22) & 0x01);
    info->mwait = (uint8_t)((edx >> 23) & 0x01);
    info->slsm = (uint8_t)((edx >> 24) & 0x01);
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    /* --- Sub Leaf 2 --- */
    const cpuid_raw_t *leaf72 = cpuid_raw_find(db, 7, 2);
    if (leaf72 == NULL) {
        return;
    }
    edx = leaf72->edx;
    // EDX
    info->psfd = (uint8_t)((edx >> 0) & 0x01);
    info->ipred_ctrl = (uint8_t)((edx >> 1) & 0x01);
    info->rrsba_ctrl = (uint8_t)((edx >> 2) & 0x01);
    info->ddpu_u = (uint8_t)((edx >> 3) & 0x01);
    info->bhi_ctrl = (uint8_t)((edx >> 4) & 0x01);
    info->mcdt_no = (uint8_t)((edx >> 5) & 0x01);
    info->uc_lock_disable = (uint8_t)((edx >> 6) & 0x01);
    info->monitor_mitg_no = (uint8_t)((edx >> 7) & 0x01);
}
/* ==========================================================================
 *                                                                          *
 * cpuid_decode()                                                           *
 *                                                                          *
 * Decode CPUID leaves                                                      *
 *                                                                          *
 ========================================================================== */
void cpuid_decode(cpuid_raw_db_t *db, cpu_info_t *info) {
    cpuid_decode_vendor(db, info);
    cpuid_decode_brand(db, info);
    cpuid_decode_psn(db, info);
    cpuid_decode_proc(db, &info->proc);
    cpuid_decode_feat(db, &info->features);
    cpuid_decode_ext_feat(db, &info->features_ext);

}

void cpuid_init(cpu_info_t *info) {
    (void)info;
    cpuid_raw_pass();
    cpuid_dump_db();
    cpuid_decode(&db, info);
    cpuid_decode_feat7(&db, &info->feat7);
}

static int kscope_cpuid_init(void) {
    cpuid_init(&info);
    return 0;
}

kscope_node_t cpuid_node = {
    .name = "x86-cpuid",
    .id = 0x14,
    .class = KSCOPE_CLASS_CORE,
    .subclass = KSCOPE_SUBCLASS_CORE_CPUID,
    .provides = (const char *[]){"proc.id", "proc.features", "shed.topology"},
    .provide_count = 3,
    .init = kscope_cpuid_init,
};

#define PRINT_FEAT(field, desc) \
    do { \
        if (feat->field) \
            printk("  " desc "\n"); \
    } while (0)

static void dump_feat(cpuid_feat_t *feat) {
    printk("\nFeatures\n");
    
    PRINT_FEAT(fpu,          "Onboard x87 FPU");
    PRINT_FEAT(vme,          "Virtual mode extensions");
    PRINT_FEAT(de,           "Debugging extensions (CR4 bit 3)");
    PRINT_FEAT(pse,          "Page Size Extension");
    PRINT_FEAT(tsc,          "Time Stamp Counter");
    PRINT_FEAT(msr,          "Model-specific registers");
    PRINT_FEAT(pae,          "Physical Address Extension");
    PRINT_FEAT(mce,          "Machine Check Exception");
    PRINT_FEAT(cx8,          "Compare-and-exchange 8B instruction");
    PRINT_FEAT(apic,         "Onboard APIC");
    
    PRINT_FEAT(sep,          "SYSENTER/SYSEXIT");
    PRINT_FEAT(mtrr,         "Memory Type Range Registers");
    PRINT_FEAT(pge,          "Page Global Enable");
    PRINT_FEAT(mca,          "Machine Check Architecture");
    PRINT_FEAT(cmov,         "CMOV/FCMOV instructions");
    PRINT_FEAT(pat,          "Page Attribute Table");
    PRINT_FEAT(pse36,        "36-bit Page Size Extension");
    PRINT_FEAT(psn,          "Processor Serial Number");
    PRINT_FEAT(clfsh,        "CLFLUSH instruction");
    PRINT_FEAT(nx,           "NX bit");
    PRINT_FEAT(ds,           "Debug Store");
    PRINT_FEAT(acpi,         "Thermal control MSRs");
    PRINT_FEAT(mmx,          "MMX");
    PRINT_FEAT(fxsr,         "FXSAVE/FXRSTOR");
    PRINT_FEAT(sse,          "SSE");
    PRINT_FEAT(sse2,         "SSE2");
    PRINT_FEAT(ss,           "Self-snoop");
    PRINT_FEAT(htt,          "Hyper-Threading");
    PRINT_FEAT(tm,           "Thermal Monitor");
    PRINT_FEAT(ia64,         "IA64 emulation");
    PRINT_FEAT(pbe,          "Pending Break Enable");
    // ECX
    PRINT_FEAT(sse3,         "SSE3");
    PRINT_FEAT(pclmulqdq,    "PCLMULQDQ");
    PRINT_FEAT(dtes64,       "64-bit Debug Store");
    PRINT_FEAT(monitor,      "MONITOR/MWAIT");
    PRINT_FEAT(ds_cpl,       "CPL-qualified Debug Store");
    PRINT_FEAT(vmx,          "Intel VT-x");
    PRINT_FEAT(smx,          "Safer Mode Extensions");
    PRINT_FEAT(est,          "Enhanced SpeedStep");
    PRINT_FEAT(tm2,          "Thermal Monitor 2");
    PRINT_FEAT(ssse3,        "SSSE3");
    PRINT_FEAT(cntx_id,      "L1 Context ID");
    PRINT_FEAT(sdbg,         "Silicon Debug");
    PRINT_FEAT(fma,          "FMA");
    PRINT_FEAT(cx16,         "Compare-and-exchange 16B instruction");
    PRINT_FEAT(xtpr,         "xTPR Update Control");
    PRINT_FEAT(pdcm,         "PerfMon & Debug Capability");

    PRINT_FEAT(pcid,         "Process Context Identifiers");
    PRINT_FEAT(dca,          "Direct Cache Access");
    PRINT_FEAT(sse4_1,       "SSE4.1");
    PRINT_FEAT(sse4_2,       "SSE4.2");
    PRINT_FEAT(x2apic,       "x2APIC");
    PRINT_FEAT(movbe,        "MOVBE");
    PRINT_FEAT(popcnt,       "POPCNT");
    PRINT_FEAT(tsc_deadline, "TSC Deadline Timer");
    PRINT_FEAT(aes_ni,       "AES-NI");
    PRINT_FEAT(xsave,        "XSAVE");
    PRINT_FEAT(osxsave,      "OSXSAVE");
    PRINT_FEAT(avx,          "AVX");
    PRINT_FEAT(f16c,         "F16C");
    PRINT_FEAT(rdrnd,        "RDRAND");
    PRINT_FEAT(hypervisor,   "Hypervisor Present");
}

static void dump_ext_feat(cpuid_ext_feat_t *feat) {
    printk("\nExtended features\n");

    // EDX
    PRINT_FEAT(fpu,             "Onboard x87 FPU");
    PRINT_FEAT(vme,             "Virtual mode extensions");
    PRINT_FEAT(de,              "Debugging extensions (CR4 bit 3)");
    PRINT_FEAT(pse,             "Page Size Extension");
    PRINT_FEAT(tsc,             "Time Stamp Counter");
    PRINT_FEAT(msr,             "Model-specific registers");
    PRINT_FEAT(pae,             "Physical Address Extension");
    PRINT_FEAT(mce,             "Machine Check Exception");
    PRINT_FEAT(cx8,             "compare-and-swap instruction");
    PRINT_FEAT(apic,            "Onboard APIC");
    PRINT_FEAT(syscall_k6,      "SYSCALL/SYSRET (K6)");
    PRINT_FEAT(syscall,         "SYSCALL/SYSRET");
    PRINT_FEAT(mtrr,            "Memory Type Range Registers");
    PRINT_FEAT(pge,             "Page Global Enable");
    PRINT_FEAT(mca,             "Machine Check Architecture");
    PRINT_FEAT(cmov,            "CMOV/FCMOV instructions");
    PRINT_FEAT(pat,             "Page Attribute Table");
    PRINT_FEAT(pse36,           "36-bit Page Size Extension");
    PRINT_FEAT(ecc_k7,          "ECC (K7)");
    PRINT_FEAT(ecc,             "ECC");
    PRINT_FEAT(nx,              "NX bit");
    PRINT_FEAT(sem,             "SEM (AMD legacy feature)");
    PRINT_FEAT(mmxext,          "Extended MMX");
    PRINT_FEAT(mmx,             "MMX");
    PRINT_FEAT(fxsr,            "FXSAVE/FXRSTOR");
    PRINT_FEAT(fxsr_opt,        "FXSAVE/FXRSTOR optimizations");
    PRINT_FEAT(pdpe1gb,         "1 GiB Pages");
    PRINT_FEAT(rdtscp,          "RDTSCP");
    PRINT_FEAT(rex32_k8,        "REX prefix (K8)");
    PRINT_FEAT(lm,              "Long Mode");
    PRINT_FEAT(tdnowext,        "Extended 3DNow!");
    PRINT_FEAT(tdnow,           "3DNow!");

    // ECX
    PRINT_FEAT(lahf_lm,         "LAHF/SAHF in Long Mode");
    PRINT_FEAT(cmp_legacy,      "CMP Legacy");
    PRINT_FEAT(svm,             "Secure Virtual Machine");
    PRINT_FEAT(extapic,         "Extended APIC Space");
    PRINT_FEAT(cr8_legacy,      "CR8 in 32-bit Mode");
    PRINT_FEAT(abm,             "Advanced Bit Manipulation");
    PRINT_FEAT(sse4a,           "SSE4a");
    PRINT_FEAT(misalignsse,     "Misaligned SSE");
    PRINT_FEAT(tdnowprefetch,   "PREFETCH/PREFETCHW");
    PRINT_FEAT(osvw,            "OS Visible Workaround");
    PRINT_FEAT(ibs,             "Instruction Based Sampling");
    PRINT_FEAT(xop,             "XOP");
    PRINT_FEAT(skinit,          "SKINIT/STGI");
    PRINT_FEAT(wdt,             "Watchdog Timer");
    PRINT_FEAT(tbm0,            "TBM0");
    PRINT_FEAT(lwp,             "Lightweight Profiling");
    PRINT_FEAT(fma4,            "FMA4");
    PRINT_FEAT(tce,             "Translation Cache Extension");
    PRINT_FEAT(cvt16,           "FP16/FP32 Conversion (XOP)");
    PRINT_FEAT(nodeid_msr,      "NodeID MSR");
    PRINT_FEAT(tbm,             "Trailing Bit Manipulation");
    PRINT_FEAT(topoext,         "Topology Extensions");
    PRINT_FEAT(perfctr_core,    "Core Performance Counter Extensions");
    PRINT_FEAT(perfctr_nb,      "Northbridge Performance Counter Extensions");
    PRINT_FEAT(StreamPerfMon,   "Streaming Performance Monitor");
    PRINT_FEAT(dbx,             "Data Breakpoint Extensions");
    PRINT_FEAT(perftsc,         "Performance Timestamp Counter");
    PRINT_FEAT(pcx_l2i_l3,      "AMD L2i/L3 Performance Counters");
    PRINT_FEAT(monitorx,        "MONITORX/MWAITX");
    PRINT_FEAT(addr_mask_ext,   "Address Mask Extension");

    printk("\n");
}

static void dump_feat7(cpuid_feat7_t *feat) {
    printk("\nL7 Features\n");
    if (feat->mawau != 0) {
        printk("  MPX Address-Width Adjust: 0x%x\n", feat->mawau);
    }
    PRINT_FEAT(fsgsbase, "RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE");
    PRINT_FEAT(tsc_adjust, "TSC Adjust MSR");
    PRINT_FEAT(sgx, "Software Guard Extensions");
    PRINT_FEAT(bmi1, "Bit Manipulation Instructions 1");
    PRINT_FEAT(hle, "Hardware Lock Elision");
    PRINT_FEAT(avx2, "AVX2");
    PRINT_FEAT(fdp_excptn_only, "x87 FDP updated only on exceptions");
    PRINT_FEAT(smep, "Supervisor Mode Execution Prevention");
    PRINT_FEAT(bmi2, "Bit Manipulation Instructions 2");
    PRINT_FEAT(erms, "Enhanced REP MOVSB/STOSB");
    PRINT_FEAT(invpcid, "INVPCID instruction");
    PRINT_FEAT(rtm, "Restricted Transactional Memory");
    PRINT_FEAT(rdt_m_pqm, "Platform QoS Monitoring");
    PRINT_FEAT(fcs_fds_deprecation, "FCS/FDS Deprecation");
    PRINT_FEAT(mpx, "Memory Protection Extensions");
    PRINT_FEAT(rdt_a_pqe, "Platform QoS Enforcement");
    PRINT_FEAT(avx512_f, "AVX-512 Foundation");
    PRINT_FEAT(avx512_dq, "AVX-512 Doubleword/Quadword");
    PRINT_FEAT(rdseed, "RDSEED");
    PRINT_FEAT(adx, "Multi-precision Add-Carry Instructions");
    PRINT_FEAT(smap, "Supervisor Mode Access Prevention");
    PRINT_FEAT(avx512_ifma, "AVX-512 Integer Fused Multiply-Add");
    PRINT_FEAT(pmcommit, "PCOMMIT instruction");
    PRINT_FEAT(clflushopt, "CLFLUSHOPT");
    PRINT_FEAT(clwb, "Cache Line Write Back");
    PRINT_FEAT(pt, "Intel Processor Trace");
    PRINT_FEAT(avx512_pf, "AVX-512 Prefetch");
    PRINT_FEAT(avx512_er, "AVX-512 Exponential/Reciprocal");
    PRINT_FEAT(avx512_cd, "AVX-512 Conflict Detection");
    PRINT_FEAT(sha, "SHA Instructions");
    PRINT_FEAT(avx512_bw, "AVX-512 Byte/Word");
    PRINT_FEAT(avx512_vl, "AVX-512 Vector Length");
    PRINT_FEAT(prefetchwt1, "PREFETCHWT1");
    PRINT_FEAT(avx512_vbmi, "AVX-512 Vector Byte Manipulation");
    PRINT_FEAT(umip, "User-Mode Instruction Prevention");
    PRINT_FEAT(pku, "Protection Keys for User Pages");
    PRINT_FEAT(ospke, "OS Protection Keys Enabled");
    PRINT_FEAT(waitpkg, "WAITPKG instructions");
    PRINT_FEAT(avx512_vmbi2, "AVX-512 VBMI2");
    PRINT_FEAT(cet_ss, "Control-flow Enforcement Shadow Stack");
    PRINT_FEAT(gfni, "Galois Field Instructions");
    PRINT_FEAT(vaes, "Vector AES");
    PRINT_FEAT(vpclmulqdq, "Vector Carry-less Multiply");
    PRINT_FEAT(avx512_vnni, "AVX-512 Vector Neural Network Instructions");
    PRINT_FEAT(avx512_bitalg, "AVX-512 Bit Algorithms");
    PRINT_FEAT(tme_en, "Total Memory Encryption");
    PRINT_FEAT(avx512_vpopcntdq, "AVX-512 Vector Population Count");
    PRINT_FEAT(fzm, "Fast Zero-length MOVSB");
    PRINT_FEAT(la57, "57-bit Linear Addresses");
    PRINT_FEAT(rdpid, "RDPID instruction");
    PRINT_FEAT(kl, "Key Locker");
    PRINT_FEAT(bus_lock_detect, "Bus Lock Detection");
    PRINT_FEAT(cldemote, "CLDEMOTE");
    PRINT_FEAT(mprr, "Memory Protection Range Registers");
    PRINT_FEAT(movdiri, "MOVDIRI");
    PRINT_FEAT(movdir64b, "MOVDIR64B");
    PRINT_FEAT(enqcmd, "ENQCMD");
    PRINT_FEAT(sgx_lc, "SGX Launch Configuration");
    PRINT_FEAT(pks4, "Protection Keys for Supervisor");
    PRINT_FEAT(sgx_term, "SGX Trusted EREMOVE");
    PRINT_FEAT(sgx_keys, "SGX Attestation Keys");
    PRINT_FEAT(avx512_4vnniw, "AVX-512 4VNNIW");
    PRINT_FEAT(avx512_4fmaps, "AVX-512 4FMAPS");
    PRINT_FEAT(fsrm, "Fast Short REP MOV");
    PRINT_FEAT(uintr, "User Interrupts");
    PRINT_FEAT(avx512_vp2intersect, "AVX-512 VP2INTERSECT");
    PRINT_FEAT(srbds_ctrl, "SRBDS Mitigation");
    PRINT_FEAT(md_clear, "MD_CLEAR");
    PRINT_FEAT(rtm_always_abort, "RTM Always Aborts");
    PRINT_FEAT(rtm_force_abort, "RTM Force Abort");
    PRINT_FEAT(serialize, "SERIALIZE");
    PRINT_FEAT(hybrid, "Hybrid Processor");
    PRINT_FEAT(tsxldtrk, "TSX Load Tracking");
    PRINT_FEAT(pconfig, "PCONFIG");
    PRINT_FEAT(lbr, "Architectural Last Branch Records");
    PRINT_FEAT(cet_ibt, "Control-flow Enforcement Indirect Branch Tracking");
    PRINT_FEAT(iamx_bf16, "iAMX BF16");
    PRINT_FEAT(avx512_fp16, "AVX-512 FP16");
    PRINT_FEAT(iamx_tile, "iAMX Tile");
    PRINT_FEAT(iamx_int8, "iAMX INT8");
    PRINT_FEAT(spec_ctrl, "Speculation Control");
    PRINT_FEAT(stibp, "Single Thread Indirect Branch Predictors");
    PRINT_FEAT(l1d_flush, "L1 Data Cache Flush");
    PRINT_FEAT(arch_capabilities, "Architectural Capabilities MSR");
    PRINT_FEAT(core_capabilities, "Core Capabilities MSR");
    PRINT_FEAT(ssbd, "Speculative Store Bypass Disable");
    PRINT_FEAT(sha512, "SHA-512 Instructions");
    PRINT_FEAT(sm3, "SM3 Instructions");
    PRINT_FEAT(sm4, "SM4 Instructions");
    PRINT_FEAT(rao_int, "Remote Atomic Operations");
    PRINT_FEAT(amx_vnni, "iAMX VNNI");
    PRINT_FEAT(avx512_bf16, "AVX-512 BF16");
    PRINT_FEAT(lass, "Linear Address Space Separation");
    PRINT_FEAT(cmpccxadd, "CMPccXADD");
    PRINT_FEAT(archperfmonext, "Architectural Performance Monitor Extensions");
    PRINT_FEAT(dedup, "Memory Deduplication");
    PRINT_FEAT(fzrm, "Fast Zero REP MOV");
    PRINT_FEAT(fsrs, "Fast Short REP STOS");
    PRINT_FEAT(rsrcs, "Return Stack Controls");
    PRINT_FEAT(fred, "Flexible Return and Event Delivery");
    PRINT_FEAT(lkgs, "Load Key from GS");
    PRINT_FEAT(wrmsrns, "Non-serializing WRMSR");
    PRINT_FEAT(nmi_src, "NMI Source Reporting");
    PRINT_FEAT(iamx_fp16, "iAMX FP16");
    PRINT_FEAT(hreset, "History Reset");
    PRINT_FEAT(avx_ifma, "AVX Integer Fused Multiply-Add");
    PRINT_FEAT(lam, "Linear Address Masking");
    PRINT_FEAT(msrlist, "MSR List");
    PRINT_FEAT(invd_disable_post_bios_done, "INVD Disable after BIOS");
    PRINT_FEAT(movrs, "MOVRS");
    PRINT_FEAT(ppin, "Protected Processor Inventory Number");
    PRINT_FEAT(pbndkb, "PBNDKB");
    PRINT_FEAT(cpuid_maxval_lim_rmv, "CPUID MaxVal Limit Removed");
    PRINT_FEAT(mpsadbw_512, "AVX-512 MPSADBW");
    PRINT_FEAT(avx512_rao_fp, "AVX-512 Remote Atomic FP");
    PRINT_FEAT(rdt_m_asym, "Asymmetric QoS Monitoring");
    PRINT_FEAT(rdt_a_asym, "Asymmetric QoS Enforcement");
    PRINT_FEAT(reduced_isa, "Reduced ISA");
    PRINT_FEAT(sipi64, "64-bit SIPI");
    PRINT_FEAT(msr_imm, "Immediate MSR Access");
    PRINT_FEAT(ace, "Authenticated Code Execution");
    PRINT_FEAT(avx512_vnni_fp16, "AVX-512 VNNI FP16");
    PRINT_FEAT(avx512_vnni_int8, "AVX-512 VNNI INT8");
    PRINT_FEAT(avx512_ne_convert, "AVX-512 Neural Convert");
    PRINT_FEAT(avx_vnni_int8, "AVX VNNI INT8");
    PRINT_FEAT(avx_ne_convert, "AVX Neural Convert");
    PRINT_FEAT(iamx_complex, "iAMX Complex");
    PRINT_FEAT(avx_vnni_int16, "AVX VNNI INT16");
    PRINT_FEAT(avx512_vnni_int16, "AVX-512 VNNI INT16");
    PRINT_FEAT(utmr, "User Timer");
    PRINT_FEAT(prefetchi, "PREFETCHI");
    PRINT_FEAT(user_msr, "User-mode MSRs");
    PRINT_FEAT(avx512_bf16_ne, "AVX-512 BF16 Neural Extensions");
    PRINT_FEAT(uiret_uif_from_rflags, "UIRET Restores UIF");
    PRINT_FEAT(cet_sss, "CET Supervisor Shadow Stack");
    PRINT_FEAT(avx10, "AVX10");
    PRINT_FEAT(apx_f, "Advanced Performance Extensions");
    PRINT_FEAT(sec_tee_attestation, "Secure TEE Attestation");
    PRINT_FEAT(mwait, "MWAIT");
    PRINT_FEAT(slsm, "Supervisor Linear Speculation Mitigation");
    PRINT_FEAT(psfd, "Predictive Store Forwarding Disable");
    PRINT_FEAT(ipred_ctrl, "Indirect Prediction Control");
    PRINT_FEAT(rrsba_ctrl, "RRSBA Control");
    PRINT_FEAT(ddpu_u, "Data Dependent Prefetcher Update");
    PRINT_FEAT(bhi_ctrl, "Branch History Injection Control");
    PRINT_FEAT(mcdt_no, "No MXCSR Configuration Dependent Timing");
    PRINT_FEAT(uc_lock_disable, "UC Lock Disable");
    PRINT_FEAT(monitor_mitg_no, "No MONITOR Mitigation Required");
    printk("\n");
}

static void dump_proc(cpuid_proc_info_t *proc) {
    printk("\nProcessor info\n");
    printk("  Stepping:          0x%x\n", proc->stepping);
    printk("  Model:             0x%x\n", proc->model);
    printk("  Family:            0x%x\n", proc->family);
    printk("  Processor Type:    0x%x\n", proc->proc_type);
    printk("  Ext Model:         0x%x\n", proc->ext_model);
    printk("  Ext Family:        0x%x\n", proc->ext_family);
    printk("  Display Model:     0x%x\n", proc->display_model);
    printk("  Display Family:    0x%x\n", proc->display_family);
    printk("  Brand Index:       0x%x\n", proc->brand_index);
    printk("  Cache Line Size:   0x%xB\n", proc->clflush_line_size);
    printk("  Max Logical IDs:   0x%x\n", proc->max_logical_ids);
    printk("  Initial APIC ID:   0x%x\n\n", proc->initial_apic_id);
}

/* ==========================================================================
 *                                                                          *
 * cpuid_dump()                                                             *
 *                                                                          *
 * dump CPUID leaves                                                        *
 *                                                                          *
 ========================================================================== */
void cpuid_dump(cpu_info_t *info) {
    /*
        * VGA text mode renders \t as ⚬, so I use spaces
    */
	printk("\n=== CPU ===\n\n");
    printk("Vendor:            %s\n", info->vendor);
    printk("Brand:             %s\n", info->brand);
    if (info->features.psn) {
        printk("PSN:               %08x%08x%08x\n", info->psn[0], info->psn[1], info->psn[2]);
    }
    dump_proc(&info->proc);
    dump_feat(&info->features);
    dump_ext_feat(&info->features_ext);
    dump_feat7(&info->feat7);
}