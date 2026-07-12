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
// static inline void u32_to_bytes(uint32_t val, char *buf);

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
// static inline void u32_to_bytes(uint32_t val, char *buf) {
//     buf[0] = (char)(val & 0xFF);
//     buf[1] = (char)((val >> 8)  & 0xFF);
//     buf[2] = (char)((val >> 16) & 0xFF);
//     buf[3] = (char)((val >> 24) & 0xFF);
// }

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

/* ==========================================================================
 *                                                                          *
 * cpuid_decode()                                                           *
 *                                                                          *
 * Decode CPUID leaves                                                      *
 *                                                                          *
 ========================================================================== */
void cpuid_decode(cpuid_raw_db_t *db, cpu_info_t *info) {

}

void cpuid_init(cpu_info_t *info) {
    (void)info;
    cpuid_raw_pass();
    cpuid_dump_db();
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
