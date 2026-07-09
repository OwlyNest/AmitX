/*
 * arch/x86/cpuid.c - CPU identification layer
 * Author:   amity
 * Date:     Tue Jul  7 14:55:12 2026
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
#include <arch/x86/cpuid.h>
#include <screen/printk.h>
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/
cpu_info_t info = { 0 };
/* --- Prototypes ---*/
static inline void cpuid_exec(uint32_t eax, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d);
static inline void cpuid_exec_sub(uint32_t eax, uint32_t ecx, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d);
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
 * cpuid_exec()                                                             *
 *                                                                          *
 * Execute CPUID with EAX=leaf and return all four registers                *
 *                                                                          *
 ========================================================================== */
static inline void cpuid_exec(uint32_t eax, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    uint32_t ra, rb, rc, rd;

    __asm__ __volatile__ (
        "cpuid"
        : "=a"(ra),
          "=b"(rb),
          "=c"(rc),
          "=d"(rd)
        : "a"(eax),
          "c"(0)
        : "cc"
    );

    *a = ra;
    *b = rb;
    *c = rc;
    *d = rd;
}

/* ==========================================================================
 *                                                                          *
 * cpuid_exec_sub()                                                         *
 *                                                                          *
 * Execute CPUID with EAX=leaf, ECX=subleaf and return all four registers   *
 *                                                                          *
 ========================================================================== */
static inline void cpuid_exec_sub(uint32_t eax, uint32_t ecx, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
#ifdef __x86_64__
    uint32_t rb;

    __asm__ __volatile__ (
        "pushq %%rbx\n\t"
        "cpuid\n\t"
        "movl %%ebx, %1\n\t"
        "popq %%rbx"
        : "=a"(*a), "=r"(rb), "=c"(*c), "=d"(*d)
        : "a"(eax), "c"(ecx)
        : "cc", "memory"
    );

    *b = rb;
#else
    uint32_t rb;

    __asm__ __volatile__ (
        "pushl %%ebx\n\t"
        "cpuid\n\t"
        "movl %%ebx, %1\n\t"
        "popl %%ebx"
        : "=a"(*a), "=r"(rb), "=c"(*c), "=d"(*d)
        : "a"(eax), "c"(ecx)
        : "cc", "memory"
    );

    *b = rb;
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
 * cpuid_get_vendor()                                                       *
 *                                                                          *
 * Read the 12-character vendor string                                      *
 * CPUID leaf 0 returns the vendor in EBX, EDX, ECX (in that order)       *
 *                                                                          *
 ========================================================================== */
void cpuid_get_vendor(char *vendor) {
    uint32_t regs[4];

    cpuid_exec(0, &regs[0], &regs[1], &regs[2], &regs[3]);
    // regs[1]=EBX, regs[3]=EDX, regs[2]=ECX
    u32_to_bytes(regs[1], &vendor[0]);
    u32_to_bytes(regs[3], &vendor[4]);
    u32_to_bytes(regs[2], &vendor[8]);
    vendor[12] = '\0';
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_brand()                                                        *
 *                                                                          *
 * Read the processor brand string if available                             *
 * Extended leaf 0x80000000 returns the max extended leaf in EAX            *
 * If it is >= 0x80000004, brand strings are in leaves                      *
 * 0x80000002 through 0x80000004 (48 bytes total)                           *
 *                                                                          *
 ========================================================================== */
void cpuid_get_brand(char *brand) {
    uint32_t regs[4];
    uint32_t max_extended;
    int i;

    brand[0] = '\0';

    cpuid_exec(0x80000000, &max_extended, &regs[1], &regs[2],
               &regs[3]);

    if (max_extended < 0x80000004) {
        return;                         // Brand string not supported
    }

    for (i = 0; i < 3; i++) {
        cpuid_exec(0x80000002 + i, &regs[0], &regs[1], &regs[2],
                   &regs[3]);

        u32_to_bytes(regs[0], &brand[i * 16 + 0]);
        u32_to_bytes(regs[1], &brand[i * 16 + 4]);
        u32_to_bytes(regs[2], &brand[i * 16 + 8]);
        u32_to_bytes(regs[3], &brand[i * 16 + 12]);
    }
    brand[48] = '\0';
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_features()                                                     *
 *                                                                          *
 * Read feature flags from CPUID leaf 1                                     *
 *                                                                          *
 ========================================================================== */
void cpuid_get_features(uint32_t *ecx, uint32_t *edx) {
    uint32_t eax, ebx;

    cpuid_exec(1, &eax, &ebx, ecx, edx);
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_proc_info()                                                    *
 *                                                                          *
 * Decode processor family, model, stepping from leaf 1                     *
 * Intel/AMD compute "display" family/model for IDs >= 0x0F                   *
 *                                                                          *
 ========================================================================== */
void cpuid_get_proc_info(cpuid_proc_info_t *info) {
    uint32_t eax, ebx, ecx, edx;
    uint32_t family, model;

    cpuid_exec(1, &eax, &ebx, &ecx, &edx);

    info->stepping      = (uint8_t)(eax & 0x0F);
    info->model         = (uint8_t)((eax >> 4) & 0x0F);
    info->family        = (uint8_t)((eax >> 8) & 0x0F);
    info->proc_type     = (uint8_t)((eax >> 12) & 0x03);
    info->ext_model     = (uint8_t)((eax >> 16) & 0x0F);
    info->ext_family    = (uint8_t)((eax >> 20) & 0xFF);

    info->brand_index   = (uint8_t)(ebx & 0xFF);
    info->clflush_line_size = (uint8_t)(((ebx >> 8) & 0xFF) * 8);
    info->max_logical_ids = (uint8_t)((ebx >> 16) & 0xFF);
    info->initial_apic_id = (ebx >> 24) & 0xFF;

    // Intel/AMD display rules: if family == 6 or 15, display model
    // is (ext_model << 4) + model. If family == 15, display family
    // is ext_family + family.
    family = info->family;
    model  = info->model;

    if (family == 0x06 || family == 0x0F) {
        model = (info->ext_model << 4) | info->model;
    }
    if (family == 0x0F) {
        family = info->ext_family + info->family;
    }

    info->display_model  = (uint8_t)model;
    info->display_family = (uint8_t)family;
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_cache_info()                                                   *
 *                                                                          *
 * Decode cache topology from leaf 4 (index = ECX sub-leaf)                 *
 * Returns 0 if the sub-leaf is invalid (EAX[4:0] == 0)                    *
 *                                                                          *
 ========================================================================== */
int cpuid_get_cache_info(uint32_t index, cpuid_cache_info_t *info) {
    uint32_t eax, ebx, ecx, edx;

    cpuid_exec_sub(4, index, &eax, &ebx, &ecx, &edx);

    info->cache_type = (uint8_t)(eax & 0x1F);
    if (info->cache_type == 0) {
        return 0;                       // No more caches
    }

    info->cache_level       = (uint8_t)((eax >> 5) & 0x07);
    info->self_initializing = (uint8_t)((eax >> 8) & 0x01);
    info->fully_associative = (uint8_t)((eax >> 9) & 0x01);
    info->max_threads_sharing = ((eax >> 14) & 0x0FFF) + 1;
    info->max_cores_sharing   = ((eax >> 26) & 0x3F) + 1;

    info->line_size       = (ebx & 0x0FFF) + 1;
    info->line_partitions = ((ebx >> 12) & 0x03FF) + 1;
    info->ways            = ((ebx >> 22) & 0x3FF) + 1;

    if (!info->fully_associative) {
        info->sets = ecx + 1;
    } else {
        info->sets = 0;
    }

    info->wbinvd         = (uint8_t)((edx >> 0) & 0x01);
    info->inclusive      = (uint8_t)((edx >> 1) & 0x01);
    info->complex_indexing = (uint8_t)((edx >> 2) & 0x01);

    return 1;
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_mwait_info()                                                   *
 *                                                                          *
 * Decode MONITOR/MWAIT features from leaf 5                              *
 *                                                                          *
 ========================================================================== */
void cpuid_get_mwait_info(cpuid_mwait_info_t *info) {
    uint32_t eax, ebx, ecx, edx;

    cpuid_exec(5, &eax, &ebx, &ecx, &edx);

    info->smallest_line = eax & 0xFFFF;
    info->largest_line  = ebx & 0xFFFF;

    info->extensions = (uint8_t)((ecx >> 0) & 0x01);
    info->interrupts_as_break = (uint8_t)((ecx >> 1) & 0x01);
    info->monitorless = (uint8_t)((ecx >> 3) & 0x01);

    info->c0_substates = (edx >> 0) & 0x0F;
    info->c1_substates = (edx >> 4) & 0x0F;
    info->c2_substates = (edx >> 8) & 0x0F;
    info->c3_substates = (edx >> 12) & 0x0F;
    info->c4_substates = (edx >> 16) & 0x0F;
    info->c5_substates = (edx >> 20) & 0x0F;
    info->c6_substates = (edx >> 24) & 0x0F;
    info->c7_substates = (edx >> 28) & 0x0F;
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_thermal_info()                                                 *
 *                                                                          *
 * Decode thermal and power management from leaf 6                          *
 *                                                                          *
 ========================================================================== */
void cpuid_get_thermal_info(cpuid_thermal_info_t *info) {
    uint32_t eax, ebx, ecx, edx;

    cpuid_exec(6, &eax, &ebx, &ecx, &edx);

    info->digital_temp_sensor = (uint8_t)(eax & 0x01);
    info->turbo_boost         = (uint8_t)((eax >> 1) & 0x01);
    info->arat                = (uint8_t)((eax >> 2) & 0x01);
    info->pln                 = (uint8_t)((eax >> 4) & 0x01);
    info->ecmd                = (uint8_t)((eax >> 5) & 0x01);
    info->ptm                 = (uint8_t)((eax >> 6) & 0x01);

    info->interrupt_thresholds = ebx & 0x0F;

    info->hardware_coordination = (uint8_t)(ecx & 0x01);
    info->energy_perf_bias      = (uint8_t)((ecx >> 3) & 0x01);
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_feat7()                                                        *
 *                                                                          *
 * Decode extended features from leaf 7, sub-leaf 0                         *
 * Caller must ensure max_basic_leaf >= 7                                   *
 *                                                                          *
 ========================================================================== */
void cpuid_get_feat7(cpuid_feat7_t *info) {
    uint32_t eax;

    cpuid_exec_sub(7, 0, &eax, &info->ebx, &info->ecx, &info->edx);
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_addr_size()                                                    *
 *                                                                          *
 * Decode virtual/physical address sizes from leaf 0x80000008               *
 *                                                                          *
 ========================================================================== */
void cpuid_get_addr_size(cpuid_addr_size_t *info) {
    uint32_t eax, ebx, ecx, edx;

    cpuid_exec(0x80000008, &eax, &ebx, &ecx, &edx);

    info->phys_addr_bits  = (uint8_t)(eax & 0xFF);
    info->lin_addr_bits   = (uint8_t)((eax >> 8) & 0xFF);
    info->guest_phys_bits = (uint8_t)((eax >> 16) & 0xFF);

    info->clzero           = (uint8_t)((ebx >> 0) & 0x01);
    info->rstr_fp_err_ptrs = (uint8_t)((ebx >> 2) & 0x01);
    info->wbnoinvd         = (uint8_t)((ebx >> 9) & 0x01);

    info->max_nc        = (ecx & 0xFF) + 1;
    info->apicid_size   = (uint8_t)((ecx >> 12) & 0x0F);
    info->perf_tsc_size = (uint8_t)((ecx >> 16) & 0x03);
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_feat()                                                     *
 *                                                                          *
 * Decode Extended features from leaf 0x80000001                            *
 *                                                                          *
 ========================================================================== */
void cpuid_get_feat(cpuid_feat_t *info) {
    uint32_t ecx, edx;
    uint32_t dummy;
    cpuid_exec(1, &dummy, &dummy, &ecx, &edx);


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
void cpuid_get_ext_feat(cpuid_ext_feat_t *info) {
    uint32_t eax, ebx, ecx, edx;

    cpuid_exec(0x80000001, &eax, &ebx, &ecx, &edx);

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

/* ==========================================================================
 *                                                                          *
 * cpuid_get_svm_info()                                                     *
 *                                                                          *
 * Decode AMD SVM features from leaf 0x8000000A                             *
 *                                                                          *
 ========================================================================== */
void cpuid_get_svm_info(cpuid_svm_info_t *info) {
    uint32_t eax, ebx, ecx, edx;

    cpuid_exec(0x8000000A, &eax, &ebx, &ecx, &edx);

    info->svm_revision = eax & 0xFF;
    info->nasid        = ebx;

    info->nested_paging   = (uint8_t)((edx >> 0) & 0x01);
    info->lbr_virt      = (uint8_t)((edx >> 1) & 0x01);
    info->svm_lock      = (uint8_t)((edx >> 2) & 0x01);
    info->nrip_save     = (uint8_t)((edx >> 3) & 0x01);
    info->tsc_rate_msr  = (uint8_t)((edx >> 4) & 0x01);
    info->vmcb_clean    = (uint8_t)((edx >> 5) & 0x01);
    info->flush_by_asid = (uint8_t)((edx >> 6) & 0x01);
    info->decode_assists = (uint8_t)((edx >> 7) & 0x01);
    info->pcm_virt       = (uint8_t)((edx >> 8) & 0x01);
    // reserved
    info->pause_filter  = (uint8_t)((edx >> 10) & 0x01);
    info->emp           =(uint8_t)((edx >> 11) & 0x01);
    info->pft           = (uint8_t)((edx >> 12) & 0x01);
    info->avic          = (uint8_t)((edx >> 13) & 0x01);
    // reserved
    info->v_vmsave_vmload = (uint8_t)((edx >> 15) & 0x01);
    info->vgif            = (uint8_t)((edx >> 16) & 0x01);
    info->gmet            = (uint8_t)((edx >> 17) & 0x01);
    info->x2avic          = (uint8_t)((edx >> 18) & 0x01);
    info->ssscheck        = (uint8_t)((edx >> 19) & 0x01);
    info->spec_ctrl       = (uint8_t)((edx >> 20) & 0x01);
    info->rogpt           = (uint8_t)((edx >> 21) & 0x01);
    // reserved
    info->host_mce_override = (uint8_t)((edx >> 23) & 0x01);
    info->tlbictl           = (uint8_t)((edx >> 24) & 0x01);
    info->vnmi              = (uint8_t)((edx >> 25) & 0x01);
    info->lbsvirt           = (uint8_t)((edx >> 26) & 0x01);
    info->extlvtoffsetfaultchg = (uint8_t)((edx >> 27) & 0x01);
    info->vmcbaddrchkchg       = (uint8_t)((edx >> 28) & 0x01);
    info->blt                  = (uint8_t)((edx >> 29) & 0x01);
    info->ihi                  = (uint8_t)((edx >> 30) & 0x01);
    info->esi                  = (uint8_t)((edx >> 31) & 0x01);

    info->gpcmef        = (uint8_t)((ecx >> 3) & 0x01);
    info->pml           = (uint8_t)((ecx >> 4) & 0x01);
    // reserved
    info->x2avic_ext    = (uint8_t)((ecx >> 6) & 0x01);
    // reserved
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_topology()                                                     *
 *                                                                          *
 * Decode Intel extended topology from leaf 0x0B                            *
 * Call with level = 0, 1, 2, ... until ECX[15:8] == 0                     *
 * Returns 0 when the level is invalid                                     *
 *                                                                          *
 ========================================================================== */
int cpuid_get_topology(uint32_t level, cpuid_topology_t *info) {
    uint32_t eax, ebx, ecx, edx;

    cpuid_exec_sub(0x0B, level, &eax, &ebx, &ecx, &edx);

    info->level_type = (uint8_t)((ecx >> 8) & 0xFF);
    if (info->level_type == 0) {
        return 0;                       // Invalid level
    }

    info->logical_per_level = ebx & 0xFFFF;
    info->level_number    = ecx & 0xFF;
    info->x2apic_id       = edx;
    info->valid           = 1;

    return 1;
}

/* ==========================================================================
 *                                                                          *
 * cpuid_get_freq()                                                         *
 *                                                                          *
 * Decode frequency info from leaves 0x15 and 0x16                          *
 * Leaf 0x15: EAX=tsc_denominator, EBX=tsc_numerator,                     *
 *            ECX=core crystal clock (Hz)                                   *
 * Leaf 0x16: EAX=base freq, EBX=max freq, ECX=bus freq (all MHz)           *
 *                                                                          *
 ========================================================================== */
void cpuid_get_freq(uint32_t max_basic, cpuid_freq_info_t *info) {
    uint32_t eax15, ebx15, ecx15, edx15;
    uint32_t eax16, ebx16, ecx16, edx16;

    info->tsc_denominator = 0;
    info->tsc_numerator   = 0;
    info->tsc_freq_hz     = 0;
    info->base_freq_mhz   = 0;
    info->max_freq_mhz    = 0;
    info->bus_freq_mhz    = 0;

    if (max_basic >= 0x15) {
        cpuid_exec(0x15, &eax15, &ebx15, &ecx15, &edx15);

        if (eax15 != 0 && ebx15 != 0) {
            info->tsc_denominator = eax15;
            info->tsc_numerator   = ebx15;
            if (ecx15 != 0) {
                info->tsc_freq_hz =
                    ((uint64_t)ebx15 * ecx15) / eax15;
            }
        }
    }

    if (max_basic >= 0x16) {
        cpuid_exec(0x16, &eax16, &ebx16, &ecx16, &edx16);

        if (eax16 != 0) {
            info->base_freq_mhz = eax16;
        }
        if (ebx16 != 0) {
            info->max_freq_mhz = ebx16;
        }
        if (ecx16 != 0) {
            info->bus_freq_mhz = ecx16;
        }
    }
}

/* ==========================================================================
 *                                                                          *
 * cpuid_init()                                                             *
 *                                                                          *
 * Fill a cpu_info_t structure with CPU information                         *
 *                                                                          *
 ========================================================================== */
void cpuid_init(cpu_info_t *info) {
    uint32_t dummy;
    uint32_t max_basic;

    cpuid_get_vendor(info->vendor);

    cpuid_exec(0, &max_basic, &dummy, &dummy, &dummy);
    info->max_basic_leaf    = max_basic;
    info->max_extended_leaf = 0;

    cpuid_exec(0x80000000, &info->max_extended_leaf, &dummy,
               &dummy, &dummy);

    cpuid_get_brand(info->brand);


    cpuid_exec(1, &info->psn[0], &dummy, &info->features_ecx, &info->features_edx);
    cpuid_exec(3, &dummy, &dummy, &info->psn[1], &info->psn[2]);

    cpuid_get_proc_info(&info->proc);

    info->has_leaf7 = 0;
    if (max_basic >= 7) {
        cpuid_get_feat7(&info->feat7);
        info->has_leaf7 = 1;
    }

    info->has_leaf80000008 = 0;
    if (info->max_extended_leaf >= 0x80000008) {
        cpuid_get_addr_size(&info->addr_size);
        info->has_leaf80000008 = 1;
    }

    info->features_ext_ecx = 0;
    info->features_ext_edx = 0;
    if (info->max_extended_leaf >= 0x80000001) {
        uint32_t a, b;
        cpuid_exec(0x80000001, &a, &b, &info->features_ext_ecx, &info->features_ext_edx);
        cpuid_get_feat(&info->features);
        cpuid_get_ext_feat(&info->features_ext);
    }

    info->has_leaf8000000a = 0;
    if (info->max_extended_leaf >= 0x8000000A) {
        info->has_leaf8000000a = 1;
    }

    info->has_leafb = 0;
    if (max_basic >= 0x0B) {
        info->has_leafb = 1;
    }

    info->has_leaf15 = 0;
    info->has_leaf16 = 0;
    if (max_basic >= 0x15) {
        cpuid_get_freq(max_basic, &info->freq);
        info->has_leaf15 = 1;
    }
    if (max_basic >= 0x16) {
        info->has_leaf16 = 1;
    }

    info->cache_count = 0;
    for (uint32_t i = 0; i < 8; i++) {
        if (!cpuid_get_cache_info(i, &info->caches[i])) {
            break;
        }
        info->cache_count++;
        info->has_thermal = 1;
    }
    if (info->has_thermal) {
        cpuid_get_thermal_info(&info->thermal);
    }
    
    // Intel topology
    info->topo_count = 0;
    if (max_basic >= 0x0B) {
        for (uint32_t i = 0; i < 8; i++) {
            if (!cpuid_get_topology(i, &info->topo[i])) {
                break;
            }
            info->topo_count++;
        }
        info->has_topology = 1;
    }
    
    info->has_mwait = 0;
    if (max_basic > 5) {
        cpuid_get_mwait_info(&info->mwait);
        info->has_mwait = 1;
    }

    // AMD SVM
    if (info->max_extended_leaf >= 0x8000000A) {
        cpuid_get_svm_info(&info->svm);
        info->has_svm = 1;
    }
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

/* ==========================================================================
 *                                                                          *
 * cpuid_dump()                                                             *
 *                                                                          *
 * Dump CPU information                                                     *
 *                                                                          *
 ========================================================================== */
static void proc_dump(cpuid_proc_info_t *proc) {
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

static void dump_feat_7(cpuid_feat7_t *leaf7) {
    printk("\nLeaf 7\n");
    printk("  Leaf 7 EBX: 0x%x\n", leaf7->ebx);
    printk("  Leaf 7 ECX: 0x%x\n", leaf7->ecx);
    printk("  Leaf 7 EDX 0x%x\n\n", leaf7->edx);
}

static void dump_addr_size(cpuid_addr_size_t *addr) {
    printk("\nAddress Size\n");
    printk("  Physical address bits:                       0x%x\n", addr->phys_addr_bits);
    printk("  Linear address bits:                         0x%x\n", addr->lin_addr_bits);
    printk("  Guest physical bits:                         0x%x\n", addr->guest_phys_bits);
    printk("  CLZERO:                                      0x%x\n", addr->clzero);
    printk("  Restore x87 Floating Point Error Pointers:   0x%x\n", addr->rstr_fp_err_ptrs);
    printk("  Write Back and Do Not Invalidate Cache Line: 0x%x\n", addr->wbnoinvd);
    printk("  Maximum number of non-coherent nodes:        0x%x\n", addr->max_nc);
    printk("  APIC ID size:                                0x%x\n", addr->apicid_size);
    printk("  Perf TSC size:                               0x%x\n\n", addr->perf_tsc_size);
}

static void dump_freq(uint8_t has_leaf_16, cpuid_freq_info_t *freq) {
    printk("\nFrequency\n");
    printk("  TSC denominator: 0x%x\n", freq->tsc_denominator);
    printk("  TSC Numerator:   0x%x\n", freq->tsc_numerator);
    printk("  TSC frequency:   %dHz\n", freq->tsc_freq_hz);
    if (has_leaf_16) {
        printk("  Base frequency:  %dmHz\n", freq->base_freq_mhz);
        printk("  Max frequency:   %dmHz\n", freq->max_freq_mhz);
        printk("  Bus frequency:   %dmHz\n", freq->bus_freq_mhz);
    }
    printk("\n");
}

static void dump_cache(int i, cpuid_cache_info_t *cache) {
    uint32_t size = (cache->line_size * cache->line_partitions * cache->ways * cache->sets); /* Bytes */
    printk("\nCache topology L%d\n", i);
    printk("  Cache type:          0x%x\n", cache->cache_type);
    printk("  Cache level:         0x%x\n", cache->cache_level);
    if (size >= 1024 * 1024) {
        /* If cache size is in megabytes, print in megabytes */
        printk("  Size:                %dMiB\n", size / (1024 * 1024));
    } else if (size >= 1024) {
        /* If cache size is not in megabytes, but kilobytes, print in kilobytes*/
        printk("  Size:                %dkiB\n", size / (1024));
    } else {
        /* Just print the ammount of bytes */
        printk("  Size:                %dB\n", size);
    }
    printk("  Self initializing:   0x%x\n", cache->self_initializing);
    printk("  Fully associative:   0x%x\n", cache->fully_associative);
    printk("  Max threads sharing: 0x%x\n", cache->max_threads_sharing);
    printk("  Max cores sharing:   0x%x\n", cache->max_cores_sharing);
    printk("  Line size:           0x%x\n", cache->line_size);
    printk("  Line partitions:     0x%x\n", cache->line_partitions);
    printk("  Ways:                0x%x\n", cache->ways);
    printk("  Sets:                0x%x\n", cache->sets);
    printk("  Cache type:          0x%x\n", cache->wbinvd);
    printk("  Inclusive:           0x%x\n", cache->inclusive);
    printk("  Complex indexing:    0x%x\n\n", cache->complex_indexing);
}

static void dump_thermal(cpuid_thermal_info_t *therm) {
    printk("\nThermal\n");
    printk("  Digital thermal sensor:                  0x%x\n", therm->digital_temp_sensor);
    printk("  Intel turbo boost:                       0x%x\n", therm->turbo_boost);
    printk("  Always Running APIC Timer:               0x%x\n", therm->arat);
    printk("  Power Limit Notification:                0x%x\n", therm->pln);
    printk("  Extended Clock Modulation Duty:          0x%x\n", therm->ecmd);
    printk("  Package Thermal Management:              0x%x\n", therm->ptm);
    printk("  Number of Interrupt Thresholds in DTS:   0x%x\n", therm->interrupt_thresholds);
    printk("  Effective frequency interface supported: 0x%x\n", therm->hardware_coordination);
    printk("  Performance-energy-bias:                 0x%x\n\n", therm->energy_perf_bias);
}

static void dump_topology(int i, cpuid_topology_t *topo) {
    printk("\nTopology %d\n", i);
    printk("  Level type: %d\n", topo->level_type);
    printk("  Logical per level: %d\n", topo->logical_per_level);
    printk("  Level number: 0x%x\n", topo->level_number);
    printk("  x2APIC ID: 0x%x\n", topo->x2apic_id);
    printk("  Valid: %d\n\n", topo->valid);
}

static void dump_mwait(cpuid_mwait_info_t *mwait) {
    printk("\nMwait\n");
    printk("  Smallest monitor line %dB\n", mwait->smallest_line);
    printk("  Largest monitor line: %dB\n", mwait->largest_line);
    printk("  Extensions:           0x%x\n", mwait->extensions);
    printk("  Interrupts as break:  0x%x\n", mwait->interrupts_as_break);
    printk("  Monitorless:          0x%x\n", mwait->monitorless);
    printk("  C0 substates:         %d\n", mwait->c0_substates);
    printk("  C1 substates:         %d\n", mwait->c1_substates);
    printk("  C2 substates:         %d\n", mwait->c2_substates);
    printk("  C3 substates:         %d\n", mwait->c3_substates);
    printk("  C4 substates:         %d\n", mwait->c4_substates);
    printk("  C5 substates:         %d\n", mwait->c5_substates);
    printk("  C6 substates:         %d\n", mwait->c6_substates);
    printk("  C7 substates:         %d\n\n", mwait->c7_substates);
}

static void dump_svm(cpuid_svm_info_t *svm) {
    printk("\nSVM\n");
    printk("  SVM revision:                                                    %d\n", svm->svm_revision);
    printk("  Number of available ASIDs:                                       %d\n", svm->nasid);
    printk("  Nested paging:                                                   %d\n", svm->nested_paging);
    printk("  Last branch records:                                             0x%x\n", svm->lbr_virt);
    printk("  SVM lock:                                                        0x%x\n", svm->svm_lock);
    printk("  nRIP save:                                                       0x%x\n", svm->nrip_save);
    printk("  MSR-based TSC rate control:                                      0x%x\n", svm->tsc_rate_msr);
    printk("  VMCB clean bits:                                                 0x%x\n", svm->vmcb_clean);
    printk("  Flush by ASID:                                                   0x%x\n", svm->flush_by_asid);
    printk("  Decode assists:                                                  0x%x\n", svm->decode_assists);
    printk("  PCM virtualization:                                              0x%x\n", svm->pcm_virt);
    printk("  PAUSE intercept filter:                                          0x%x\n", svm->pause_filter);
    printk("  Encrypted Microcode Patch:                                       0x%x\n", svm->emp);
    printk("  PAUSE filter cycle count threshold:                              0x%x\n", svm->pft);
    printk("  AMD Advanced Virtualized Interrupt Controller:                   0x%x\n", svm->avic);
    printk("  VMSAVE and VMLOAD virtualization:                                0x%x\n", svm->v_vmsave_vmload);
    printk("  Global Interrupt Flag virtualization:                            0x%x\n", svm->vgif);
    printk("  Guest Mode Execution Trap:                                       0x%x\n", svm->gmet);
    printk("  x2APIC mode supported for AVIC:                                  0x%x\n", svm->x2avic);
    printk("  SVM Supervisor shadow stack restrictions:                        0x%x\n", svm->ssscheck);
    printk("  SPEC_CTRL virtualization:                                        0x%x\n", svm->spec_ctrl);
    printk("  Read-Only Guest Page Table:                                      0x%x\n", svm->rogpt);
    printk("  Guest mode Machine-check exceptions:                             0x%x\n", svm->host_mce_override);
    printk("  INVLPGB/TLBSYNC hypervisor enable in VMCB and TLBSYNC intercept: 0x%x\n", svm->tlbictl);
    printk("  Non-Maskable interrupt virtualization:                           0x%x\n", svm->vnmi);
    printk("  Instruction-Based Sampling virtualization:                       0x%x\n", svm->lbsvirt);
    printk("  Read/Write fault behavior for extended LVT offsets:              0x%x\n", svm->extlvtoffsetfaultchg);
    printk("  VMCB address check change:                                       0x%x\n", svm->vmcbaddrchkchg);
    printk("  Bus Lock Threshold                                               0x%x\n", svm->blt);
    printk("  Idle HLT Intercept:                                              0x%x\n", svm->ihi);
    printk("  Enhanced Shutdown Intercept:                                     0x%x\n", svm->esi);
    printk("  Guest PMC Event Filtering:                                       0x%x\n", svm->gpcmef);
    printk("  Page Modification Logging:                                       0x%x\n", svm->pml);
    printk("  4096 vCPUs supported in x2AVIC mode:                             0x%x\n\n", svm->x2avic_ext);
}

static void dump_feat(cpuid_feat_t *feat) {
    printk("\nFeatures\n");
    
    printk("  Onboard x87 FPU:                                               0x%x\n", feat->fpu);
    printk("  Virtual mode extensions:                                       0x%x\n", feat->vme);
    printk("  Debugging extensions (CR4 bit 3):                              0x%x\n", feat->de);
    printk("  Page Size Extension:                                           0x%x\n", feat->pse);
    printk("  Time Stamp Counter:                                            0x%x\n", feat->tsc);
    printk("  Model-specific registers:                                      0x%x\n", feat->msr);
    printk("  Physical Address Extension:                                    0x%x\n", feat->pae);
    printk("  Machine Check Exception:                                       0x%x\n", feat->mce);
    printk("  compare-and-swap instruction:                                  0x%x\n", feat->cx8);
    printk("  Onboard APIC:                                                  0x%x\n", feat->apic);
    
    printk("  SYSENTER and SYSEXIT fast system call instructions:            0x%x\n", feat->sep);
    printk("  Memory Type Range Registers:                                   0x%x\n", feat->mtrr);
    printk("  Page Global Enable bit in CR4:                                 0x%x\n", feat->pge);
    printk("  Machine check architecture:                                    0x%x\n", feat->mca);
    printk("  Conditional move and FCMOV instructions:                       0x%x\n", feat->cmov);
    printk("  Page Attribute Table:                                          0x%x\n", feat->pat);
    printk("  36-bit page size extension:                                    0x%x\n", feat->pse36);
    printk("  Processor Serial Number supported and enabled:                 0x%x\n", feat->psn);
    printk("  CLFLUSH cache line flush instruction:                          0x%x\n", feat->clfsh);
    printk("  NX bit (page-table no-execute bit):                            0x%x\n", feat->nx);
    printk("  Debug store: save trace of executed jumps:                     0x%x\n", feat->ds);
    printk("  Onboard thermal control MSRs for ACPI:                         0x%x\n", feat->acpi);
    printk("  MMX instructions:                                              0x%x\n", feat->mmx);
    printk("  FXSAVE, FXRSTOR instructions:                                  0x%x\n", feat->fxsr);
    printk("  Streaming SIMD Extensions:                                     0x%x\n", feat->sse);
    printk("  SSE2 instructions:                                             0x%x\n", feat->sse2);
    printk("  CPU cache implements self-snoop:                               0x%x\n", feat->ss);
    printk("  Max APIC IDs reserved field is Valid:                          0x%x\n", feat->htt);
    printk("  Thermal monitor automatically limits temperature:              0x%x\n", feat->tm);
    printk("  IA64 processor emulating x86:                                  0x%x\n", feat->ia64);
    printk("  Pending Break Enable:                                          0x%x\n", feat->pbe);
    // ECX
    printk("  SSE3:                                                          0x%x\n", feat->sse3);
    printk("  PCLMULQDQ (carry-less multiply) instruction:                   0x%x\n", feat->pclmulqdq);
    printk("  64-bit debug store:                                            0x%x\n", feat->dtes64);
    printk("  MONITOR and MWAIT instructions:                                0x%x\n", feat->monitor);
    printk("  CPL qualified debug store:                                     0x%x\n", feat->ds_cpl);
    printk("  Virtual Machine eXtensions:                                    0x%x\n", feat->vmx);
    printk("  Safer Mode Extensions:                                         0x%x\n", feat->smx);
    printk("  Enhanced SpeedStep:                                            0x%x\n", feat->est);
    printk("  Thermal Monitor 2:                                             0x%x\n", feat->tm2);
    printk("  Supplemental SSE3 instructions:                                0x%x\n", feat->ssse3);
    printk("  L1 Context ID:                                                 0x%x\n", feat->cntx_id);
    printk("  Silicon Debug interface:                                       0x%x\n", feat->sdbg);
    printk("  Fused multiply-add:                                            0x%x\n", feat->fma);
    printk("  CMPXCHG16B instruction:                                        0x%x\n", feat->cx16);
    printk("  Can disable sending task priority messages:                    0x%x\n", feat->xtpr);
    printk("  Perfmon & debug capability:                                    0x%x\n", feat->pdcm);
    
    printk("  Process context identifiers:                                   0x%x\n", feat->pcid);
    printk("  Direct cache access for DMA writes:                            0x%x\n", feat->dca);
    printk("  SSE4.1 instructions:                                           0x%x\n", feat->sse4_1);
    printk("  SSE4.2 instructions:                                           0x%x\n", feat->sse4_2);
    printk("  x2APIC (enhanced APIC):                                        0x%x\n", feat->x2apic);
    printk("  MOVBE instruction:                                             0x%x\n", feat->movbe);
    printk("  POPCNT instruction:                                            0x%x\n", feat->popcnt);
    printk("  APIC implements one-shot operation using a TSC deadline value: 0x%x\n", feat->tsc_deadline);
    printk("  AES instruction set:                                           0x%x\n", feat->aes_ni);
    printk("  Extensible processor state save/restore:                       0x%x\n", feat->xsave);
    printk("  XSAVE enabled by OS:                                           0x%x\n", feat->osxsave);
    printk("  Advanced Vector Extensions:                                    0x%x\n", feat->avx);
    printk("  Floating-point conversion instructions to/from FP16 format:    0x%x\n", feat->f16c);
    printk("  RDRAND (on-chip random number generator) feature:              0x%x\n", feat->rdrnd);
    printk("  Hypervisor present:                                            0x%x\n\n", feat->hypervisor);
}

static void dump_ext_feat(cpuid_ext_feat_t *feat) {
    printk("\nExtended features\n");
    
    printk("  Onboard x87 FPU:                                               0x%x\n", feat->fpu);
    printk("  Virtual mode extensions:                                       0x%x\n", feat->vme);
    printk("  Debugging extensions (CR4 bit 3):                              0x%x\n", feat->de);
    printk("  Page Size Extension:                                           0x%x\n", feat->pse);
    printk("  Time Stamp Counter:                                            0x%x\n", feat->tsc);
    printk("  Model-specific registers:                                      0x%x\n", feat->msr);
    printk("  Physical Address Extension:                                    0x%x\n", feat->pae);
    printk("  Machine Check Exception:                                       0x%x\n", feat->mce);
    printk("  compare-and-swap instruction:                                  0x%x\n", feat->cx8);
    printk("  Onboard APIC:                                                  0x%x\n", feat->apic);
    printk("  SYSCALL/SYSRET (K6):                                           0x%x\n", feat->syscall_k6);
    printk("  SYSCALL and SYSRET instructions:                               0x%x\n", feat->syscall);
    printk("  Memory Type Range Registers:                                   0x%x\n", feat->mtrr);
    printk("  Page Global Enable bit in CR4:                                 0x%x\n", feat->pge);
    printk("  Machine check architecture:                                    0x%x\n", feat->mca);
    printk("  Conditional move and FCMOV instructions:                       0x%x\n", feat->cmov);
    printk("  Page Attribute Table:                                          0x%x\n", feat->pat);
    printk("  36-bit page size extension:                                    0x%x\n", feat->pse36);
    printk("  ECC (K7):                                                      0x%x\n", feat->ecc_k7);
    printk("  ECC:                                                           0x%x\n", feat->ecc);
    printk("  NX bit (page-table no-execute bit):                            0x%x\n", feat->nx);
    printk("  (SEM)?:                                                        0x%x\n", feat->sem);
    printk("  Extended MMX:                                                  0x%x\n", feat->mmxext);
    printk("  MMX instructions:                                              0x%x\n", feat->mmx);
    printk("  FXSAVE, FXRSTOR instructions:                                  0x%x\n", feat->fxsr);
    printk("  FXSAVE/FXRSTOR optimizations:                                  0x%x\n", feat->fxsr_opt);
    printk("  GiB pages:                                                     0x%x\n", feat->pdpe1gb);
    printk("  RDTSCP instruction:                                            0x%x\n", feat->rdtscp);
    printk("  REX prefix (K8):                                               0x%x\n", feat->rex32_k8);
    printk("  Long mode:                                                     0x%x\n", feat->lm);
    printk("  Extended 3DNow!:                                               0x%x\n", feat->tdnowext);
    printk("  3DNow!:                                                        0x%x\n", feat->tdnow);
    // ECX
    printk("  LAHF/SAHF in long mode:                                        0x%x\n", feat->lahf_lm);
    printk("  Hyperthreading not valid:                                      0x%x\n", feat->cmp_legacy);
    printk("  Secure Virtual Machine:                                        0x%x\n", feat->svm);
    printk("  Extended APIC space:                                           0x%x\n", feat->extapic);
    printk("  CR8 in 32-bit mode:                                            0x%x\n", feat->cr8_legacy);
    printk("  Advanced bit manipulation:                                     0x%x\n", feat->abm);
    printk("  SSE4a:                                                         0x%x\n", feat->sse4a);
    printk("  Misaligned SSE mode:                                           0x%x\n", feat->misalignsse);
    printk("  PREFETCH and PREFETCHW instructions:                           0x%x\n", feat->tdnowprefetch);
    printk("  OS Visible Workaround:                                         0x%x\n", feat->osvw);
    printk("  Instruction Based Sampling:                                    0x%x\n", feat->ibs);
    printk("  XOP instruction set:                                           0x%x\n", feat->xop);
    printk("  SKINIT and STGI instructions:                                  0x%x\n", feat->skinit);
    printk("  Watchdog timer:                                                0x%x\n", feat->wdt);
    printk("  TBM0 instruction support:                                      0x%x\n", feat->tbm0);
    printk("  Lightweight Profiling:                                         0x%x\n", feat->lwp);
    printk("  4-operand fused multiply-add instructions:                     0x%x\n", feat->fma4);
    printk("  Translation Cache Extension:                                   0x%x\n", feat->tce);
    printk("  XOP-prefix forms of the FP16 <-> FP32 conversion instructions: 0x%x\n", feat->cvt16);
    printk("  NodeID MSR:                                                    0x%x\n", feat->nodeid_msr);
    
    printk("  Trailing Bit Manipulation:                                     0x%x\n", feat->tbm);
    printk("  Topology Extensions:                                           0x%x\n", feat->topoext);
    printk("  Core performance counter extensions:                           0x%x\n", feat->perfctr_core);
    printk("  Northbridge performance counter extensions:                    0x%x\n", feat->perfctr_nb);
    printk("  Streaming performance monitor architecture:                    0x%x\n", feat->StreamPerfMon);
    printk("  Data breakpoint extensions:                                    0x%x\n", feat->dbx);
    printk("  Performance timestamp counter:                                 0x%x\n", feat->perftsc);
    printk("  AMD Fam perf counter extensions (L2i/L3):                      0x%x\n", feat->pcx_l2i_l3);
    printk("  MONITORX and MWAITX instructions:                              0x%x\n", feat->monitorx);
    printk("  Address mask extension:                                        0x%x\n\n", feat->addr_mask_ext);
}

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
	printk("Max basic leaf:    0x%x\n", info->max_basic_leaf);
	printk("Max extended leaf: 0x%x\n", info->max_extended_leaf);
	
    printk("Features ECX:      0x%x\n", info->features_ecx);
    printk("Features EXT ECX:  0x%x\n", info->features_ext_ecx);
	printk("Features EDX:      0x%x\n", info->features_edx);
    printk("Features EXT ECX:  0x%x\n", info->features_ext_ecx);
    dump_feat(&info->features);
    dump_ext_feat(&info->features_ext);

    proc_dump(&info->proc);

    if (info->has_leaf7) {
        dump_feat_7(&info->feat7);
    }

    if (info->has_leaf80000008) {
        dump_addr_size(&info->addr_size);
    }

    if (info->has_leaf15) {
        dump_freq(info->has_leaf16, &info->freq);
    }

    if (info->has_thermal) {
        for (uint32_t i = 0; i < info->cache_count; i++) {
            dump_cache(i, &info->caches[i]);
        }
        dump_thermal(&info->thermal);
    }
    if (info->has_topology) {
        for (uint32_t i = 0; i < info->topo_count; i++) {
            dump_topology(i, &info->topo[i]);
        }
    }
    if (info->has_mwait) {
        dump_mwait(&info->mwait);
    }
    if (info->features_ext.svm) {
        dump_svm(&info->svm);
    }

	printk("\n===========\n\n");
}