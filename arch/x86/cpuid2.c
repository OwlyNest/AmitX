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

/* --- Includes ---*/
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/phonon_macros.h>
#include <arch/x86/cpuid.h>
#include <screen/printk.h>
#include <stdint.h>
/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
cpu_info_t info = { 0 };
cpuid_raw_db_t db = { 0 };

static const ULONG simple_basic[] = {
    0x00000001,
    0x00000002,
    0x00000003,

    0x00000005,
    0x00000006,
    // 
    0x00000015,
    0x00000016
};

static const ULONG simple_extended[] = {
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
static inline void cpuid_exec(cpuid_raw_db_t *db, ULONG leaf, ULONG subleaf);
static inline void u32_to_bytes(ULONG val, CHAR *buf);

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
    ULONGLONG orig, mod;

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
    ULONG orig, mod;

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
static const cpuid_raw_t *cpuid_raw_find(const cpuid_raw_db_t *db, ULONG leaf, ULONG subleaf) {
	for (ULONG i = 0; i < db->count; i++) {
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
static inline void cpuid_exec(cpuid_raw_db_t *db, ULONG leaf, ULONG subleaf) {
	ULONG in_subleaf = subleaf;
#ifdef __x86_64__
        ULONG ra, rb, rc, rd;
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
        ULONG ra, rb, rc, rd;
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
 * Convert a little-endian ULONG to 4 CHAR's                              *
 *                                                                          *
 ========================================================================== */
static inline void u32_to_bytes(ULONG val, CHAR *buf) {
    buf[0] = (CHAR)(val & 0xFF);
    buf[1] = (CHAR)((val >> 8)  & 0xFF);
    buf[2] = (CHAR)((val >> 16) & 0xFF);
    buf[3] = (CHAR)((val >> 24) & 0xFF);
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
	ULONG max_basic = cpuid_raw_find(&db, 0, 0)->eax;
    for (size_t i = 0; i < ARRAY_SIZE(simple_basic); i++) {
        if (max_basic >= simple_basic[i]) {
            cpuid_exec(&db, simple_basic[i], 0);
        }
    }

    for (ULONG idx = 0;; idx++) {
        cpuid_exec(&db, 4, idx);
    
        if ((cpuid_raw_find(&db, 4, idx)->eax & 0x1F) == 0) {
            break;
        }
    }

    cpuid_exec(&db, 0x00000007, 0);
    for (ULONG idx = 1; idx <= cpuid_raw_find(&db, 7, 0)->eax; idx++) {
        cpuid_exec(&db, 7, idx);
    }

    for (ULONG idx = 0;; idx++) {
        cpuid_exec(&db, 0x0B, idx);
    
        ULONG ecx = cpuid_raw_find(&db, 0x0B, idx)->ecx;
        ULONG level_type = (ecx >> 8) & 0xFF;
    
        if (level_type == 0) {
            break;
        }
    }

	cpuid_exec(&db, 0x0D, 0);
    cpuid_exec(&db, 0x0D, 1);
    const cpuid_raw_t *leaf = cpuid_raw_find(&db, 0x0D, 0);
    ULONGLONG bitmap = ((ULONGLONG)leaf->edx << 32) | leaf->eax;
    for (ULONG i = 2; i < 64; i++) {
        if (bitmap & (1ULL << i)) {
            cpuid_exec(&db, 0x0D, i);
        }
    }

    
	cpuid_exec(&db, 0x80000000, 0);
	ULONG max_extended = cpuid_raw_find(&db, 0x80000000, 0)->eax;
    for (SIZE_T i = 0; i < ARRAY_SIZE(simple_extended); i++) {
        if (max_extended >= simple_extended[i]) {
            cpuid_exec(&db, simple_extended[i], 0);
        }
    }

    if (max_extended >= 0x8000001D) {
        for (ULONG idx = 0;; idx++) {
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
    for (ULONG idx = 0; idx < db.count; idx++) {
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

    u32_to_bytes(leaf->ebx, (CHAR *)&info->vendor[0]);
    u32_to_bytes(leaf->edx, (CHAR *)&info->vendor[4]);
    u32_to_bytes(leaf->ecx, (CHAR *)&info->vendor[8]);
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

        u32_to_bytes(brand_leaf->eax, (CHAR *)&info->brand[i * 16 + 0]);
        u32_to_bytes(brand_leaf->ebx, (CHAR *)&info->brand[i * 16 + 4]);
        u32_to_bytes(brand_leaf->ecx, (CHAR *)&info->brand[i * 16 + 8]);
        u32_to_bytes(brand_leaf->edx, (CHAR *)&info->brand[i * 16 + 12]);
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

    info->stepping      = (BYTE)( leaf1->eax        & 0x0F);
    info->model         = (BYTE)((leaf1->eax >> 4)  & 0x0F);
    info->family        = (BYTE)((leaf1->eax >> 8)  & 0x0F);
    info->proc_type     = (BYTE)((leaf1->eax >> 12) & 0x03);
    info->ext_model     = (BYTE)((leaf1->eax >> 16) & 0x0F);
    info->ext_family    = (BYTE)((leaf1->eax >> 20) & 0xFF);

    info->brand_index       = (BYTE)(leaf1->ebx & 0xFF);
    info->clflush_line_size = (BYTE)(((leaf1->ebx >> 8) & 0xFF) * 8);
    info->max_logical_ids   = (BYTE)((leaf1->ebx >> 16) & 0xFF);
    info->initial_apic_id   = (leaf1->ebx >> 24) & 0xFF;

    // Intel/AMD display rules: if family == 6 or 15, display model
    // is (ext_model << 4) + model. If family == 15, display family
    // is ext_family + family.
    ULONG family = info->family;
    ULONG model  = info->model;

    if (family == 0x06 || family == 0x0F) {
        model = (info->ext_model << 4) | info->model;
    }
    if (family == 0x0F) {
        family = info->ext_family + info->family;
    }

    info->display_model  = (BYTE)model;
    info->display_family = (BYTE)family;
}

static void cpuid_decode_feat(cpuid_raw_db_t *db, cpuid_feat_t *info) {

    const cpuid_raw_t *leaf = cpuid_raw_find(db, 1, 0);

    ULONG ecx = leaf->ecx;
    ULONG edx = leaf->edx;

    info->fpu = (BYTE)((edx >> 0) & 0x01);
    info->vme = (BYTE)((edx >> 1) & 0x01);
    info->de = (BYTE)((edx >> 2) & 0x01);
    info->pse = (BYTE)((edx >> 3) & 0x01);
    info->tsc = (BYTE)((edx >> 4) & 0x01);
    info->msr = (BYTE)((edx >> 5) & 0x01);
    info->pae = (BYTE)((edx >> 6) & 0x01);
    info->mce = (BYTE)((edx >> 7) & 0x01);
    info->cx8 = (BYTE)((edx >> 8) & 0x01);
    info->apic = (BYTE)((edx >> 9) & 0x01);
    
    info->sep = (BYTE)((edx >> 11) & 0x01);
    info->mtrr = (BYTE)((edx >> 12) & 0x01);
    info->pge = (BYTE)((edx >> 13) & 0x01);
    info->mca = (BYTE)((edx >> 14) & 0x01);
    info->cmov = (BYTE)((edx >> 15) & 0x01);
    info->pat = (BYTE)((edx >> 16) & 0x01);
    info->pse36 = (BYTE)((edx >> 17) & 0x01);
    info->psn = (BYTE)((edx >> 18) & 0x01);
    info->clfsh = (BYTE)((edx >> 19) & 0x01);
    info->nx = (BYTE)((edx >> 20) & 0x01);
    info->ds = (BYTE)((edx >> 21) & 0x01);
    info->acpi = (BYTE)((edx >> 22) & 0x01);
    info->mmx = (BYTE)((edx >> 23) & 0x01);
    info->fxsr = (BYTE)((edx >> 24) & 0x01);
    info->sse = (BYTE)((edx >> 25) & 0x01);
    info->sse2 = (BYTE)((edx >> 26) & 0x01);
    info->ss = (BYTE)((edx >> 27) & 0x01);
    info->htt = (BYTE)((edx >> 28) & 0x01);
    info->tm = (BYTE)((edx >> 29) & 0x01);
    info->ia64 = (BYTE)((edx >> 30) & 0x01);
    info->pbe = (BYTE)((edx >> 31) & 0x01);

    info->sse3 = (BYTE)((ecx >> 0) & 0x01);
    info->pclmulqdq = (BYTE)((ecx >> 1) & 0x01);
    info->dtes64 = (BYTE)((ecx >> 2) & 0x01);
    info->monitor = (BYTE)((ecx >> 3) & 0x01);
    info->ds_cpl = (BYTE)((ecx >> 4) & 0x01);
    info->vmx = (BYTE)((ecx >> 5) & 0x01);
    info->smx = (BYTE)((ecx >> 6) & 0x01);
    info->est = (BYTE)((ecx >> 7) & 0x01);
    info->tm2 = (BYTE)((ecx >> 8) & 0x01);
    info->ssse3 = (BYTE)((ecx >> 9) & 0x01);
    info->cntx_id = (BYTE)((ecx >> 10) & 0x01);
    info->sdbg = (BYTE)((ecx >> 11) & 0x01);
    info->fma = (BYTE)((ecx >> 12) & 0x01);
    info->cx16 = (BYTE)((ecx >> 13) & 0x01);
    info->xtpr = (BYTE)((ecx >> 14) & 0x01);
    info->pdcm = (BYTE)((ecx >> 15) & 0x01);
    // reserved
    info->pcid = (BYTE)((ecx >> 17) & 0x01);
    info->dca = (BYTE)((ecx >> 18) & 0x01);
    info->sse4_1 = (BYTE)((ecx >> 19) & 0x01);
    info->sse4_2 = (BYTE)((ecx >> 20) & 0x01);
    info->x2apic = (BYTE)((ecx >> 21) & 0x01);
    info->movbe = (BYTE)((ecx >> 22) & 0x01);
    info->popcnt = (BYTE)((ecx >> 23) & 0x01);
    info->tsc_deadline = (BYTE)((ecx >> 24) & 0x01);
    info->aes_ni = (BYTE)((ecx >> 25) & 0x01);
    info->xsave = (BYTE)((ecx >> 26) & 0x01);
    info->osxsave = (BYTE)((ecx >> 27) & 0x01);
    info->avx = (BYTE)((ecx >> 28) & 0x01);
    info->f16c = (BYTE)((ecx >> 29) & 0x01);
    info->rdrnd = (BYTE)((ecx >> 30) & 0x01);
    info->hypervisor = (BYTE)((ecx >> 31) & 0x01);
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_ext_feat()                                                     *
 *                                                                          *
 * Decode Extended features from leaf 0x80000001                            *
 *                                                                          *
 ========================================================================== */
static void cpuid_decode_ext_feat(cpuid_raw_db_t *db, cpuid_ext_feat_t *info) {
    ULONG ecx, edx;

    const cpuid_raw_t *leaf = cpuid_raw_find(db, 0x80000001, 0);
    ecx = leaf->ecx;
    edx = leaf->edx;

    info->fpu = (BYTE)((edx >> 0) & 0x01);
    info->vme = (BYTE)((edx >> 1) & 0x01);
    info->de = (BYTE)((edx >> 2) & 0x01);
    info->pse = (BYTE)((edx >> 3) & 0x01);
    info->tsc = (BYTE)((edx >> 4) & 0x01);
    info->msr = (BYTE)((edx >> 5) & 0x01);
    info->pae = (BYTE)((edx >> 6) & 0x01);
    info->mce = (BYTE)((edx >> 7) & 0x01);
    info->cx8 = (BYTE)((edx >> 8) & 0x01);
    info->apic = (BYTE)((edx >> 9) & 0x01);
    info->syscall_k6 = (BYTE)((edx >> 10) & 0x01);
    info->syscall = (BYTE)((edx >> 11) & 0x01);
    info->mtrr = (BYTE)((edx >> 12) & 0x01);
    info->pge = (BYTE)((edx >> 13) & 0x01);
    info->mca = (BYTE)((edx >> 14) & 0x01);
    info->cmov = (BYTE)((edx >> 15) & 0x01);
    info->pat = (BYTE)((edx >> 16) & 0x01);
    info->pse36 = (BYTE)((edx >> 17) & 0x01);
    info->ecc_k7 = (BYTE)((edx >> 18) & 0x01);
    info->ecc = (BYTE)((edx >> 19) & 0x01);
    info->nx = (BYTE)((edx >> 20) & 0x01);
    info->sem = (BYTE)((edx >> 21) & 0x01);
    info->mmxext = (BYTE)((edx >> 22) & 0x01);
    info->mmx = (BYTE)((edx >> 23) & 0x01);
    info->fxsr = (BYTE)((edx >> 24) & 0x01);
    info->fxsr_opt = (BYTE)((edx >> 25) & 0x01);
    info->pdpe1gb = (BYTE)((edx >> 26) & 0x01);
    info->rdtscp = (BYTE)((edx >> 27) & 0x01);
    info->rex32_k8 = (BYTE)((edx >> 28) & 0x01);
    info->lm = (BYTE)((edx >> 29) & 0x01);
    info->tdnowext = (BYTE)((edx >> 30) & 0x01);
    info->tdnow = (BYTE)((edx >> 31) & 0x01);

    info->lahf_lm = (BYTE)((ecx >> 0) & 0x01);
    info->cmp_legacy = (BYTE)((ecx >> 1) & 0x01);
    info->svm = (BYTE)((ecx >> 2) & 0x01);
    info->extapic = (BYTE)((ecx >> 3) & 0x01);
    info->cr8_legacy = (BYTE)((ecx >> 4) & 0x01);
    info->abm = (BYTE)((ecx >> 5) & 0x01);
    info->sse4a = (BYTE)((ecx >> 6) & 0x01);
    info->misalignsse = (BYTE)((ecx >> 7) & 0x01);
    info->tdnowprefetch = (BYTE)((ecx >> 8) & 0x01);
    info->osvw = (BYTE)((ecx >> 9) & 0x01);
    info->ibs = (BYTE)((ecx >> 10) & 0x01);
    info->xop = (BYTE)((ecx >> 11) & 0x01);
    info->skinit = (BYTE)((ecx >> 12) & 0x01);
    info->wdt = (BYTE)((ecx >> 13) & 0x01);
    info->tbm0 = (BYTE)((ecx >> 14) & 0x01);
    info->lwp = (BYTE)((ecx >> 15) & 0x01);
    info->fma4 = (BYTE)((ecx >> 16) & 0x01);
    info->tce = (BYTE)((ecx >> 17) & 0x01);
    info->cvt16 = (BYTE)((ecx >> 18) & 0x01);
    info->nodeid_msr = (BYTE)((ecx >> 19) & 0x01);
    // reserved
    info->tbm = (BYTE)((ecx >> 21) & 0x01);
    info->topoext = (BYTE)((ecx >> 22) & 0x01);
    info->perfctr_core = (BYTE)((ecx >> 23) & 0x01);
    info->perfctr_nb = (BYTE)((ecx >> 24) & 0x01);
    info->StreamPerfMon = (BYTE)((ecx >> 25) & 0x01);
    info->dbx = (BYTE)((ecx >> 26) & 0x01);
    info->perftsc = (BYTE)((ecx >> 27) & 0x01);
    info->pcx_l2i_l3 = (BYTE)((ecx >> 28) & 0x01);
    info->monitorx = (BYTE)((ecx >> 29) & 0x01);
    info->addr_mask_ext = (BYTE)((ecx >> 30) & 0x01);
    //reserved
}

static void cpuid_decode_feat7(cpuid_raw_db_t *db, cpuid_feat7_t *info) {
    ULONG eax, ebx, ecx, edx;
    /* --- Sub Leaf 0 --- */
    const cpuid_raw_t *leaf70 = cpuid_raw_find(db, 7, 0);
    ebx = leaf70->ebx;
    ecx = leaf70->ecx;
    edx = leaf70->edx;
    // EBX
    info->fsgsbase = (BYTE)((ebx >> 0) & 0x01);
    info->tsc_adjust = (BYTE)((ebx >> 1) & 0x01);
    info->sgx = (BYTE)((ebx >> 2) & 0x01);
    info->bmi1 = (BYTE)((ebx >> 3) & 0x01);
    info->hle = (BYTE)((ebx >> 4) & 0x01);
    info->avx2 = (BYTE)((ebx >> 5) & 0x01);
    info->fdp_excptn_only = (BYTE)((ebx >> 6) & 0x01);
    info->smep = (BYTE)((ebx >> 7) & 0x01);
    info->bmi2 = (BYTE)((ebx >> 8) & 0x01);
    info->erms = (BYTE)((ebx >> 9) & 0x01);
    info->invpcid = (BYTE)((ebx >> 10) & 0x01);
    info->rtm = (BYTE)((ebx >> 11) & 0x01);
    info->rdt_m_pqm = (BYTE)((ebx >> 12) & 0x01);
    info->fcs_fds_deprecation = (BYTE)((ebx >> 13) & 0x01);
    info->mpx = (BYTE)((ebx >> 14) & 0x01);
    info->rdt_a_pqe = (BYTE)((ebx >> 15) & 0x01);
    info->avx512_f = (BYTE)((ebx >> 16) & 0x01);
    info->avx512_dq = (BYTE)((ebx >> 17) & 0x01);
    info->rdseed = (BYTE)((ebx >> 18) & 0x01);
    info->adx = (BYTE)((ebx >> 19) & 0x01);
    info->smap = (BYTE)((ebx >> 20) & 0x01);
    info->avx512_ifma = (BYTE)((ebx >> 21) & 0x01);
    info->pmcommit = (BYTE)((ebx >> 22) & 0x01);
    info->clflushopt = (BYTE)((ebx >> 23) & 0x01);
    info->clwb = (BYTE)((ebx >> 24) & 0x01);
    info->pt = (BYTE)((ebx >> 25) & 0x01);
    info->avx512_pf = (BYTE)((ebx >> 26) & 0x01);
    info->avx512_er = (BYTE)((ebx >> 27) & 0x01);
    info->avx512_cd = (BYTE)((ebx >> 28) & 0x01);
    info->sha = (BYTE)((ebx >> 29) & 0x01);
    info-> avx512_bw = (BYTE)((ebx >> 30) & 0x01);
    info->avx512_vl = (BYTE)((ebx >> 31) & 0x01);
    // ECX
    info->prefetchwt1 = (BYTE)((ecx >> 0) & 0x01);
    info->avx512_vbmi = (BYTE)((ecx >> 1) & 0x01);
    info->umip = (BYTE)((ecx >> 2) & 0x01);
    info->pku = (BYTE)((ecx >> 3) & 0x01);
    info->ospke = (BYTE)((ecx >> 4) & 0x01);
    info->waitpkg = (BYTE)((ecx >> 5) & 0x01);
    info->avx512_vmbi2 = (BYTE)((ecx >> 6) & 0x01);
    info->cet_ss = (BYTE)((ecx >> 7) & 0x01);
    info->gfni = (BYTE)((ecx >> 8) & 0x01);
    info->vaes = (BYTE)((ecx >> 9) & 0x01);
    info->vpclmulqdq = (BYTE)((ecx >> 10) & 0x01);
    info->avx512_vnni = (BYTE)((ecx >> 11) & 0x01);
    info->avx512_bitalg = (BYTE)((ecx >> 12) & 0x01);
    info->tme_en = (BYTE)((ecx >> 13) & 0x01);
    info->avx512_vpopcntdq = (BYTE)((ecx >> 14) & 0x01);
    info->fzm = (BYTE)((ecx >> 15) & 0x01);
    info->la57 = (BYTE)((ecx >> 16) & 0x01);
    info->mawau = (DWORD)((ecx >> 17) & 0x1F);
    info->rdpid = (BYTE)((ecx >> 22) & 0x01);
    info->kl = (BYTE)((ecx >> 23) & 0x01);
    info->bus_lock_detect = (BYTE)((ecx >> 24) & 0x01);
    info->cldemote = (BYTE)((ecx >> 25) & 0x01);
    info->mprr = (BYTE)((ecx >> 26) & 0x01);
    info->movdiri = (BYTE)((ecx >> 27) & 0x01);
    info->movdir64b = (BYTE)((ecx >> 28) & 0x01);
    info->enqcmd = (BYTE)((ecx >> 29) & 0x01);
    info->sgx_lc = (BYTE)((ecx >> 30) & 0x01);
    info->pks4 = (BYTE)((ecx >> 31) & 0x01);
    // EDX
    info->sgx_term = (BYTE)((edx >> 0) & 0x01);
    info->sgx_keys = (BYTE)((edx >> 1) & 0x01);
    info->avx512_4vnniw = (BYTE)((edx >> 2) & 0x01);
    info->avx512_4fmaps = (BYTE)((edx >> 3) & 0x01);
    info->fsrm = (BYTE)((edx >> 4) & 0x01);
    info->uintr = (BYTE)((edx >> 5) & 0x01);
    // reserved
    // reserved
    info->avx512_vp2intersect = (BYTE)((edx >> 8) & 0x01);
    info->srbds_ctrl = (BYTE)((edx >> 9) & 0x01);
    info->md_clear = (BYTE)((edx >> 10) & 0x01);
    info->rtm_always_abort = (BYTE)((edx >> 11) & 0x01);
    // reserved
    info->rtm_force_abort = (BYTE)((edx >> 13) & 0x01);
    info->serialize = (BYTE)((edx >> 14) & 0x01);
    info->hybrid = (BYTE)((edx >> 15) & 0x01);
    info->tsxldtrk = (BYTE)((edx >> 16) & 0x01);
    // reserved
    info->pconfig = (BYTE)((edx >> 18) & 0x01);
    info->lbr = (BYTE)((edx >> 19) & 0x01);
    info->cet_ibt = (BYTE)((edx >> 20) & 0x01);
    // reserved
    info->iamx_bf16 = (BYTE)((edx >> 22) & 0x01);
    info->avx512_fp16 = (BYTE)((edx >> 23) & 0x01);
    info->iamx_tile = (BYTE)((edx >> 24) & 0x01);
    info->iamx_int8 = (BYTE)((edx >> 25) & 0x01);
    info->spec_ctrl = (BYTE)((edx >> 26) & 0x01);
    info->stibp = (BYTE)((edx >> 27) & 0x01);
    info->l1d_flush = (BYTE)((edx >> 28) & 0x01);
    info->arch_capabilities = (BYTE)((edx >> 29) & 0x01);
    info->core_capabilities = (BYTE)((edx >> 30) & 0x01);
    info->ssbd = (BYTE)((edx >> 31) & 0x01);
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
    info->sha512 = (BYTE)((eax >> 0) & 0x01);
    info->sm3 = (BYTE)((eax >> 1) & 0x01);
    info->sm4 = (BYTE)((eax >> 2) & 0x01);
    info->rao_int = (BYTE)((eax >> 3) & 0x01);
    info->amx_vnni = (BYTE)((eax >> 4) & 0x01);
    info->avx512_bf16 = (BYTE)((eax >> 5) & 0x01);
    info->lass = (BYTE)((eax >> 6) & 0x01);
    info->cmpccxadd = (BYTE)((eax >> 7) & 0x01);
    info->archperfmonext = (BYTE)((eax >> 8) & 0x01);
    info->dedup = (BYTE)((eax >> 9) & 0x01);
    info->fzrm = (BYTE)((eax >> 10) & 0x01);
    info->fsrs = (BYTE)((eax >> 11) & 0x01);
    info->rsrcs = (BYTE)((eax >> 12) & 0x01);
    // reserved
    // reserved
    // reserved
    // reserved
    info->fred = (BYTE)((eax >> 17) & 0x01);
    info->lkgs = (BYTE)((eax >> 18) & 0x01);
    info->wrmsrns = (BYTE)((eax >> 19) & 0x01);
    info->nmi_src = (BYTE)((eax >> 20) & 0x01);
    info->iamx_fp16 = (BYTE)((eax >> 21) & 0x01);
    info->hreset = (BYTE)((eax >> 22) & 0x01);
    info->avx_ifma = (BYTE)((eax >> 23) & 0x01);
    // reserved
    // reserved
    info->lam = (BYTE)((eax >> 26) & 0x01);
    info->msrlist = (BYTE)((eax >> 27) & 0x01);
    // reserved
    // reserved
    info->invd_disable_post_bios_done = (BYTE)((eax >> 30) & 0x01);
    info->movrs = (BYTE)((eax >> 31) & 0x01);
    // EBX
    info->ppin = (BYTE)((ebx >> 0) & 0x01);
    info->pbndkb = (BYTE)((ebx >> 1) & 0x01);
    // reserved
    info->cpuid_maxval_lim_rmv = (BYTE)((ebx >> 3) & 0x01);
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
    info->mpsadbw_512 = (BYTE)((ebx >> 28) & 0x01);
    // reserved
    info->avx512_rao_fp = (BYTE)((ebx >> 30) & 0x01);
    // ECX
    info->rdt_m_asym = (BYTE)((ecx >> 0) & 0x01);
    info->rdt_a_asym = (BYTE)((ecx >> 1) & 0x01);
    info->reduced_isa = (BYTE)((ecx >> 2) & 0x01);
    // reserved
    info->sipi64 = (BYTE)((ecx >> 4) & 0x01);
    info->msr_imm = (BYTE)((ecx >> 5) & 0x01);
    // reserved
    // reserved
    // reserved
    // reserved
    // reserved
    info->ace = (BYTE)((ecx >> 11) & 0x01);
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
    info->avx512_vnni_fp16 = (BYTE)((edx >> 1) & 0x01);
    info->avx512_vnni_int8 = (BYTE)((edx >> 2) & 0x01);
    info->avx512_ne_convert = (BYTE)((edx >> 3) & 0x01);
    info->avx_vnni_int8 = (BYTE)((edx >> 4) & 0x01);
    info->avx_ne_convert = (BYTE)((edx >> 5) & 0x01);
    // reserved
    // reserved
    info->iamx_complex = (BYTE)((edx >> 8) & 0x01);
    // reserved
    info->avx_vnni_int16 = (BYTE)((edx >> 10) & 0x01);
    info->avx512_vnni_int16 = (BYTE)((edx >> 11) & 0x01);
    // reserved
    info->utmr = (BYTE)((edx >> 13) & 0x01);
    info->prefetchi = (BYTE)((edx >> 14) & 0x01);
    info->user_msr = (BYTE)((edx >> 15) & 0x01);
    info->avx512_bf16_ne = (BYTE)((edx >> 16) & 0x01);
    info->uiret_uif_from_rflags = (BYTE)((edx >> 17) & 0x01);
    info->cet_sss = (BYTE)((edx >> 18) & 0x01);
    info->avx10 = (BYTE)((edx >> 19) & 0x01);
    // reserved
    info->apx_f = (BYTE)((edx >> 21) & 0x01);
    info->sec_tee_attestation = (BYTE)((edx >> 22) & 0x01);
    info->mwait = (BYTE)((edx >> 23) & 0x01);
    info->slsm = (BYTE)((edx >> 24) & 0x01);
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
    info->psfd = (BYTE)((edx >> 0) & 0x01);
    info->ipred_ctrl = (BYTE)((edx >> 1) & 0x01);
    info->rrsba_ctrl = (BYTE)((edx >> 2) & 0x01);
    info->ddpu_u = (BYTE)((edx >> 3) & 0x01);
    info->bhi_ctrl = (BYTE)((edx >> 4) & 0x01);
    info->mcdt_no = (BYTE)((edx >> 5) & 0x01);
    info->uc_lock_disable = (BYTE)((edx >> 6) & 0x01);
    info->monitor_mitg_no = (BYTE)((edx >> 7) & 0x01);
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