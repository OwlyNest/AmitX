/*
	* include/arch/x86/cpuid.h - CPU identification
	* Author:   amity
	* Date:     Tue Jul  7 14:55:18 2026
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
#ifndef __ARCH_X86_CPUID_H__
#define __ARCH_X86_CPUID_H__

// Vendor strings from CPUs.
#define CPUID_VENDOR_AMD           "AuthenticAMD"
#define CPUID_VENDOR_AMD_OLD       "AMDisbetter!" // Early engineering samples of AMD K5 processor
#define CPUID_VENDOR_INTEL         "GenuineIntel"
#define CPUID_VENDOR_IOTEL         "GenuineIotel"
#define CPUID_VENDOR_VIA           "VIA VIA VIA "
#define CPUID_VENDOR_TRANSMETA     "GenuineTMx86"
#define CPUID_VENDOR_TRANSMETA_OLD "TransmetaCPU"
#define CPUID_VENDOR_CYRIX         "CyrixInstead"
#define CPUID_VENDOR_CENTAUR       "CentaurHauls"
#define CPUID_VENDOR_NEXGEN        "NexGenDriven"
#define CPUID_VENDOR_UMC           "UMC UMC UMC "
#define CPUID_VENDOR_SIS           "SiS SiS SiS "
#define CPUID_VENDOR_NSC           "Geode by NSC"
#define CPUID_VENDOR_RISE          "RiseRiseRise"
#define CPUID_VENDOR_VORTEX        "Vortex86 SoC"
#define CPUID_VENDOR_AO486         "MiSTer AO486"
#define CPUID_VENDOR_AO486_OLD     "GenuineAO486"
#define CPUID_VENDOR_ZHAOXIN       "  Shanghai  "
#define CPUID_VENDOR_HYGON         "HygonGenuine"
#define CPUID_VENDOR_ELBRUS        "E2K MACHINE "
 
// Vendor strings from hypervisors.
#define CPUID_VENDOR_QEMU          "TCGTCGTCGTCG"
#define CPUID_VENDOR_KVM           " KVMKVMKVM  "
#define CPUID_VENDOR_VMWARE        "VMwareVMware"
#define CPUID_VENDOR_VIRTUALBOX    "VBoxVBoxVBox"
#define CPUID_VENDOR_XEN           "XenVMMXenVMM"
#define CPUID_VENDOR_HYPERV        "Microsoft Hv"
#define CPUID_VENDOR_PARALLELS     " prl hyperv "
#define CPUID_VENDOR_PARALLELS_ALT " lrpepyh vr " // Sometimes Parallels incorrectly encodes "prl hyperv" as "lrpepyh vr" due to an endianness mismatch.
#define CPUID_VENDOR_BHYVE         "bhyve bhyve "
#define CPUID_VENDOR_QNX           " QNXQVMBSQG "

#define CPUID_RAW_MAX 128
/* --- Includes ---*/
#include <stdint.h>

/* --- Typedefs - Structs - Enums ---*/
typedef struct {
    uint32_t leaf;
    uint32_t subleaf;

    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpuid_raw_t;

typedef struct {
    cpuid_raw_t entries[CPUID_RAW_MAX];
    uint32_t count;
} cpuid_raw_db_t;

// Leaf 1 feature flags, ECX register.
typedef enum {
    CPUID_FEAT_ECX_SSE3               = 1U << 0,
    CPUID_FEAT_ECX_PCLMUL             = 1U << 1,
    CPUID_FEAT_ECX_DTES64             = 1U << 2,
    CPUID_FEAT_ECX_MONITOR            = 1U << 3,
    CPUID_FEAT_ECX_DS_CPL             = 1U << 4,
    CPUID_FEAT_ECX_VMX                = 1U << 5,
    CPUID_FEAT_ECX_SMX                = 1U << 6,
    CPUID_FEAT_ECX_EST                = 1U << 7,
    CPUID_FEAT_ECX_TM2                = 1U << 8,
    CPUID_FEAT_ECX_SSSE3              = 1U << 9,
    CPUID_FEAT_ECX_CID                = 1U << 10,
    CPUID_FEAT_ECX_SDBG               = 1U << 11,
    CPUID_FEAT_ECX_FMA                = 1U << 12,
    CPUID_FEAT_ECX_CX16               = 1U << 13,
    CPUID_FEAT_ECX_XTPR               = 1U << 14,
    CPUID_FEAT_ECX_PDCM               = 1U << 15,
    CPUID_FEAT_ECX_PCID               = 1U << 17,
    CPUID_FEAT_ECX_DCA                = 1U << 18,
    CPUID_FEAT_ECX_SSE4_1             = 1U << 19,
    CPUID_FEAT_ECX_SSE4_2             = 1U << 20,
    CPUID_FEAT_ECX_X2APIC             = 1U << 21,
    CPUID_FEAT_ECX_MOVBE              = 1U << 22,
    CPUID_FEAT_ECX_POPCNT             = 1U << 23,
    CPUID_FEAT_ECX_TSC_DEADLINE       = 1U << 24,
    CPUID_FEAT_ECX_AES                = 1U << 25,
    CPUID_FEAT_ECX_XSAVE              = 1U << 26,
    CPUID_FEAT_ECX_OSXSAVE            = 1U << 27,
    CPUID_FEAT_ECX_AVX                = 1U << 28,
    CPUID_FEAT_ECX_F16C               = 1U << 29,
    CPUID_FEAT_ECX_RDRAND             = 1U << 30,
    CPUID_FEAT_ECX_HYPERVISOR         = 1U << 31
} cpuid_feat_ecx_t;

// Leaf 1 feature flags, EDX register.
typedef enum {
    CPUID_FEAT_EDX_FPU                = 1U << 0,
    CPUID_FEAT_EDX_VME                = 1U << 1,
    CPUID_FEAT_EDX_DE                 = 1U << 2,
    CPUID_FEAT_EDX_PSE                = 1U << 3,
    CPUID_FEAT_EDX_TSC                = 1U << 4,
    CPUID_FEAT_EDX_MSR                = 1U << 5,
    CPUID_FEAT_EDX_PAE                = 1U << 6,
    CPUID_FEAT_EDX_MCE                = 1U << 7,
    CPUID_FEAT_EDX_CX8                = 1U << 8,
    CPUID_FEAT_EDX_APIC               = 1U << 9,
    CPUID_FEAT_EDX_SEP                = 1U << 11,
    CPUID_FEAT_EDX_MTRR               = 1U << 12,
    CPUID_FEAT_EDX_PGE                = 1U << 13,
    CPUID_FEAT_EDX_MCA                = 1U << 14,
    CPUID_FEAT_EDX_CMOV               = 1U << 15,
    CPUID_FEAT_EDX_PAT                = 1U << 16,
    CPUID_FEAT_EDX_PSE36              = 1U << 17,
    CPUID_FEAT_EDX_PSN                = 1U << 18,
    CPUID_FEAT_EDX_CLFLUSH            = 1U << 19,
    CPUID_FEAT_EDX_DS                 = 1U << 21,
    CPUID_FEAT_EDX_ACPI               = 1U << 22,
    CPUID_FEAT_EDX_MMX                = 1U << 23,
    CPUID_FEAT_EDX_FXSR               = 1U << 24,
    CPUID_FEAT_EDX_SSE                = 1U << 25,
    CPUID_FEAT_EDX_SSE2               = 1U << 26,
    CPUID_FEAT_EDX_SS                 = 1U << 27,
    CPUID_FEAT_EDX_HTT                = 1U << 28,
    CPUID_FEAT_EDX_TM                 = 1U << 29,
    CPUID_FEAT_EDX_IA64               = 1U << 30,
    CPUID_FEAT_EDX_PBE                = 1U << 31
} cpuid_feat_edx_t;

// Leaf 7, sub-leaf 0, EBX feature flags.
typedef enum {
    CPUID_FEAT7_0_EBX_FSGSBASE        = 1U << 0,
    CPUID_FEAT7_0_EBX_TSC_ADJUST      = 1U << 1,
    CPUID_FEAT7_0_EBX_SGX             = 1U << 2,
    CPUID_FEAT7_0_EBX_BMI1            = 1U << 3,
    CPUID_FEAT7_0_EBX_HLE             = 1U << 4,
    CPUID_FEAT7_0_EBX_AVX2            = 1U << 5,
    CPUID_FEAT7_0_EBX_FDP_EXCPTN_ONLY = 1U << 6,
    CPUID_FEAT7_0_EBX_SMEP            = 1U << 7,
    CPUID_FEAT7_0_EBX_BMI2            = 1U << 8,
    CPUID_FEAT7_0_EBX_ERMS            = 1U << 9,
    CPUID_FEAT7_0_EBX_INVPCID         = 1U << 10,
    CPUID_FEAT7_0_EBX_RTM             = 1U << 11,
    CPUID_FEAT7_0_EBX_RDT_M           = 1U << 12,
    CPUID_FEAT7_0_EBX_DEP_FPU_CS_DS   = 1U << 13,
    CPUID_FEAT7_0_EBX_MPX             = 1U << 14,
    CPUID_FEAT7_0_EBX_RDT_A           = 1U << 15,
    CPUID_FEAT7_0_EBX_AVX512F         = 1U << 16,
    CPUID_FEAT7_0_EBX_AVX512DQ        = 1U << 17,
    CPUID_FEAT7_0_EBX_RDSEED          = 1U << 18,
    CPUID_FEAT7_0_EBX_ADX             = 1U << 19,
    CPUID_FEAT7_0_EBX_SMAP            = 1U << 20,
    CPUID_FEAT7_0_EBX_AVX512IFMA      = 1U << 21,
    CPUID_FEAT7_0_EBX_PCOMMIT         = 1U << 22, // Deprecated
    CPUID_FEAT7_0_EBX_CLFLUSHOPT      = 1U << 23,
    CPUID_FEAT7_0_EBX_CLWB            = 1U << 24,
    CPUID_FEAT7_0_EBX_INTEL_PT        = 1U << 25,
    CPUID_FEAT7_0_EBX_AVX512PF        = 1U << 26,
    CPUID_FEAT7_0_EBX_AVX512ER        = 1U << 27,
    CPUID_FEAT7_0_EBX_AVX512CD        = 1U << 28,
    CPUID_FEAT7_0_EBX_SHA             = 1U << 29,
    CPUID_FEAT7_0_EBX_AVX512BW        = 1U << 30,
    CPUID_FEAT7_0_EBX_AVX512VL        = 1U << 31
} cpuid_feat7_0_ebx_t;

// Leaf 7, sub-leaf 0, ECX feature flags.
typedef enum {
    CPUID_FEAT7_0_ECX_PREFETCHWT1     = 1U << 0,
    CPUID_FEAT7_0_ECX_AVX512_VBMI     = 1U << 1,
    CPUID_FEAT7_0_ECX_UMIP            = 1U << 2,
    CPUID_FEAT7_0_ECX_PKU             = 1U << 3,
    CPUID_FEAT7_0_ECX_OSPKE           = 1U << 4,
    CPUID_FEAT7_0_ECX_WAITPKG         = 1U << 5,
    CPUID_FEAT7_0_ECX_AVX512_VBMI2    = 1U << 6,
    CPUID_FEAT7_0_ECX_CET_SS          = 1U << 7,
    CPUID_FEAT7_0_ECX_GFNI            = 1U << 8,
    CPUID_FEAT7_0_ECX_VAES            = 1U << 9,
    CPUID_FEAT7_0_ECX_VPCLMULQDQ      = 1U << 10,
    CPUID_FEAT7_0_ECX_AVX512_VNNI     = 1U << 11,
    CPUID_FEAT7_0_ECX_AVX512_BITALG   = 1U << 12,
    CPUID_FEAT7_0_ECX_TME             = 1U << 13,
    CPUID_FEAT7_0_ECX_AVX512_VPOPCNTDQ= 1U << 14,
    CPUID_FEAT7_0_ECX_LA57            = 1U << 16,
    CPUID_FEAT7_0_ECX_RDPID           = 1U << 22,
    CPUID_FEAT7_0_ECX_KL              = 1U << 23,
    CPUID_FEAT7_0_ECX_CLDEMOTE        = 1U << 25,
    CPUID_FEAT7_0_ECX_MOVDIRI         = 1U << 27,
    CPUID_FEAT7_0_ECX_MOVDIR64B       = 1U << 28,
    CPUID_FEAT7_0_ECX_ENQCMD          = 1U << 29,
    CPUID_FEAT7_0_ECX_SGX_LC          = 1U << 30,
    CPUID_FEAT7_0_ECX_PKS             = 1U << 31
} cpuid_feat7_0_ecx_t;

// Leaf 7, sub-leaf 0, EDX feature flags.
typedef enum {
    CPUID_FEAT7_0_EDX_AVX512_4VNNIW   = 1U << 2,
    CPUID_FEAT7_0_EDX_AVX512_4FMAPS   = 1U << 3,
    CPUID_FEAT7_0_EDX_FSRM            = 1U << 4,
    CPUID_FEAT7_0_EDX_AVX512_VP2INTERSECT = 1U << 8,
    CPUID_FEAT7_0_EDX_SRBDS_CTRL      = 1U << 9,
    CPUID_FEAT7_0_EDX_MD_CLEAR        = 1U << 10,
    CPUID_FEAT7_0_EDX_RTM_ALWAYS_ABORT= 1U << 11,
    CPUID_FEAT7_0_EDX_RTM_FORCE_ABORT = 1U << 12,
    CPUID_FEAT7_0_EDX_SERIALIZE       = 1U << 14,
    CPUID_FEAT7_0_EDX_TSXLDTRK        = 1U << 16,
    CPUID_FEAT7_0_EDX_PCONFIG         = 1U << 18,
    CPUID_FEAT7_0_EDX_ARCH_LBR        = 1U << 19,
    CPUID_FEAT7_0_EDX_CET_IBT         = 1U << 20,
    CPUID_FEAT7_0_EDX_AMX_BF16        = 1U << 22,
    CPUID_FEAT7_0_EDX_AVX512_FP16     = 1U << 23,
    CPUID_FEAT7_0_EDX_AMX_TILE        = 1U << 24,
    CPUID_FEAT7_0_EDX_AMX_INT8        = 1U << 25,
    CPUID_FEAT7_0_EDX_IBPB            = 1U << 26,
    CPUID_FEAT7_0_EDX_STIBP           = 1U << 27,
    CPUID_FEAT7_0_EDX_L1D_FLUSH       = 1U << 28,
    CPUID_FEAT7_0_EDX_ARCH_CAPABILITIES= 1U << 29,
    CPUID_FEAT7_0_EDX_CORE_CAPABILITIES = 1U << 30,
    CPUID_FEAT7_0_EDX_SSBD            = 1U << 31
} cpuid_feat7_0_edx_t;

// Leaf 0x80000001, ECX extended feature flags.
typedef enum {
    CPUID_FEAT_EXT_ECX_LAHF_LM        = 1U << 0,
    CPUID_FEAT_EXT_ECX_CMP_LEGACY     = 1U << 1,
    CPUID_FEAT_EXT_ECX_SVM            = 1U << 2,
    CPUID_FEAT_EXT_ECX_EXT_APIC_SPACE = 1U << 3,
    CPUID_FEAT_EXT_ECX_ALT_MOV_CR8    = 1U << 4,
    CPUID_FEAT_EXT_ECX_ABM            = 1U << 5,
    CPUID_FEAT_EXT_ECX_SSE4A          = 1U << 6,
    CPUID_FEAT_EXT_ECX_MISALIGN_SSE   = 1U << 7,
    CPUID_FEAT_EXT_ECX_3DNOW_PREFETCH = 1U << 8,
    CPUID_FEAT_EXT_ECX_OSVW           = 1U << 9,
    CPUID_FEAT_EXT_ECX_IBS            = 1U << 10,
    CPUID_FEAT_EXT_ECX_XOP            = 1U << 11,
    CPUID_FEAT_EXT_ECX_SKINIT         = 1U << 12,
    CPUID_FEAT_EXT_ECX_WDT            = 1U << 13,
    CPUID_FEAT_EXT_ECX_LWP            = 1U << 15,
    CPUID_FEAT_EXT_ECX_FMA4           = 1U << 16,
    CPUID_FEAT_EXT_ECX_TCE            = 1U << 17,
    CPUID_FEAT_EXT_ECX_NODEID_MSR     = 1U << 19,
    CPUID_FEAT_EXT_ECX_TBM            = 1U << 21,
    CPUID_FEAT_EXT_ECX_TOPOEXT        = 1U << 22,
    CPUID_FEAT_EXT_ECX_PERFCTR_CORE   = 1U << 23,
    CPUID_FEAT_EXT_ECX_PERFCTR_NB     = 1U << 24,
    CPUID_FEAT_EXT_ECX_DBX            = 1U << 26,
    CPUID_FEAT_EXT_ECX_PERFTSC        = 1U << 27,
    CPUID_FEAT_EXT_ECX_PCX_L2I        = 1U << 28,
    CPUID_FEAT_EXT_ECX_MONITORX       = 1U << 29,
    CPUID_FEAT_EXT_ECX_ADDR_MASK_EXT  = 1U << 30
} cpuid_feat_ext_ecx_t;

// Leaf 0x80000001, EDX extended feature flags.
typedef enum {
    CPUID_FEAT_EXT_EDX_FPU            = 1U << 0,
    CPUID_FEAT_EXT_EDX_VME            = 1U << 1,
    CPUID_FEAT_EXT_EDX_DE             = 1U << 2,
    CPUID_FEAT_EXT_EDX_PSE            = 1U << 3,
    CPUID_FEAT_EXT_EDX_TSC            = 1U << 4,
    CPUID_FEAT_EXT_EDX_MSR            = 1U << 5,
    CPUID_FEAT_EXT_EDX_PAE            = 1U << 6,
    CPUID_FEAT_EXT_EDX_MCE            = 1U << 7,
    CPUID_FEAT_EXT_EDX_CX8            = 1U << 8,
    CPUID_FEAT_EXT_EDX_APIC           = 1U << 9,
    CPUID_FEAT_EXT_EDX_SYSCALL        = 1U << 11,
    CPUID_FEAT_EXT_EDX_MTRR           = 1U << 12,
    CPUID_FEAT_EXT_EDX_PGE            = 1U << 13,
    CPUID_FEAT_EXT_EDX_MCA            = 1U << 14,
    CPUID_FEAT_EXT_EDX_CMOV           = 1U << 15,
    CPUID_FEAT_EXT_EDX_PAT            = 1U << 16,
    CPUID_FEAT_EXT_EDX_PSE36          = 1U << 17,
    CPUID_FEAT_EXT_EDX_NX             = 1U << 20,
    CPUID_FEAT_EXT_EDX_MMXEXT         = 1U << 22,
    CPUID_FEAT_EXT_EDX_MMX            = 1U << 23,
    CPUID_FEAT_EXT_EDX_FXSR           = 1U << 24,
    CPUID_FEAT_EXT_EDX_FFXSR          = 1U << 25,
    CPUID_FEAT_EXT_EDX_PAGE1GB        = 1U << 26,
    CPUID_FEAT_EXT_EDX_RDTSCP         = 1U << 27,
    CPUID_FEAT_EXT_EDX_LM             = 1U << 29,
    CPUID_FEAT_EXT_EDX_3DNOWEXT       = 1U << 30,
    CPUID_FEAT_EXT_EDX_3DNOW          = 1U << 31
} cpuid_feat_ext_edx_t;

// Processor info from leaf 1.
typedef struct {
    uint8_t  stepping;
    uint8_t  model;
    uint8_t  family;
    uint8_t  proc_type;
    uint8_t  ext_model;
    uint8_t  ext_family;
    uint8_t  display_model;         // Computed per Intel/AMD rules
    uint8_t  display_family;        // Computed per Intel/AMD rules
    uint8_t  brand_index;
    uint8_t  clflush_line_size;
    uint8_t  max_logical_ids;
    uint32_t initial_apic_id;
} cpuid_proc_info_t;

// Cache topology from leaf 4.
typedef struct {
    uint8_t  cache_type;            // 0=Null, 1=Data, 2=Inst, 3=Unified
    uint8_t  cache_level;
    uint8_t  self_initializing;
    uint8_t  fully_associative;
    uint32_t max_threads_sharing;
    uint32_t max_cores_sharing;
    uint32_t line_size;
    uint32_t line_partitions;
    uint32_t ways;
    uint32_t sets;
    uint8_t  wbinvd;
    uint8_t  inclusive;
    uint8_t  complex_indexing;
} cpuid_cache_info_t;

// MONITOR/MWAIT from leaf 5.
typedef struct {
    uint32_t smallest_line;
    uint32_t largest_line;
    uint8_t  extensions;
    uint8_t  interrupts_as_break;
    uint8_t  monitorless;
    uint32_t c0_substates;
    uint32_t c1_substates;
    uint32_t c2_substates;
    uint32_t c3_substates;
    uint32_t c4_substates;
    uint32_t c5_substates;
    uint32_t c6_substates;
    uint32_t c7_substates;
} cpuid_mwait_info_t;

// Thermal and power from leaf 6.
typedef struct {
    uint8_t  digital_temp_sensor;
    uint8_t  turbo_boost;
    uint8_t  arat;
    uint8_t  pln;
    uint8_t  ecmd;
    uint8_t  ptm;
    uint32_t interrupt_thresholds;
    uint8_t  hardware_coordination;
    uint8_t  energy_perf_bias;
} cpuid_thermal_info_t;

// Extended features from leaf 7, sub-leaf 0.
typedef struct {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpuid_feat7_t;

// Address sizes from leaf 0x80000008.
typedef struct {
    uint8_t  phys_addr_bits;
    uint8_t  lin_addr_bits;
    uint8_t  guest_phys_bits;
    uint8_t  clzero;
    uint8_t  rstr_fp_err_ptrs;
    uint8_t  wbnoinvd;
    uint32_t max_nc;                // Max physical cores in package
    uint8_t  apicid_size;
    uint8_t  perf_tsc_size;
} cpuid_addr_size_t;

// SVM features from leaf 0x8000000A (AMD).
typedef struct {
    // EAX
    uint32_t svm_revision;
    // EBX
    uint32_t nasid;
    // EDX
    uint8_t  nested_paging;
    uint8_t  lbr_virt;
    uint8_t  svm_lock;
    uint8_t  nrip_save;
    uint8_t  tsc_rate_msr;
    uint8_t  vmcb_clean;
    uint8_t  flush_by_asid;
    uint8_t  decode_assists;
    uint8_t  pcm_virt;
    uint8_t  pause_filter;
    uint8_t  emp;
    uint8_t  pft;
    uint8_t  avic;
    uint8_t  v_vmsave_vmload;
    uint8_t  vgif;
    uint8_t  gmet;
    uint8_t  x2avic;
    uint8_t  ssscheck;
    uint8_t  spec_ctrl;
    uint8_t  rogpt;
    uint8_t  host_mce_override;
    uint8_t  tlbictl;
    uint8_t  vnmi;
    uint8_t  lbsvirt;
    uint8_t  extlvtoffsetfaultchg;
    uint8_t  vmcbaddrchkchg;
    uint8_t  blt;
    uint8_t  ihi;
    uint8_t  esi;
    // ECX
    uint8_t gpcmef;
    uint8_t pml;
    uint8_t x2avic_ext;
} cpuid_svm_info_t;

typedef struct {
    // EDX
    uint8_t fpu;
    uint8_t vme;
    uint8_t de;
    uint8_t pse;
    uint8_t tsc;
    uint8_t msr;
    uint8_t pae;
    uint8_t mce;
    uint8_t cx8;
    uint8_t apic;
    // reserved
    uint8_t sep;
    uint8_t mtrr;
    uint8_t pge;
    uint8_t mca;
    uint8_t cmov;
    uint8_t pat;
    uint8_t pse36;
    uint8_t psn;
    uint8_t clfsh;
    uint8_t nx;
    uint8_t ds;
    uint8_t acpi;
    uint8_t mmx;
    uint8_t fxsr;
    uint8_t sse;
    uint8_t sse2;
    uint8_t ss;
    uint8_t htt;
    uint8_t tm;
    uint8_t ia64;
    uint8_t pbe;
    // ECX
    uint8_t sse3;
    uint8_t pclmulqdq;
    uint8_t dtes64;
    uint8_t monitor;
    uint8_t ds_cpl;
    uint8_t vmx;
    uint8_t smx;
    uint8_t est;
    uint8_t tm2;
    uint8_t ssse3;
    uint8_t cntx_id;
    uint8_t sdbg;
    uint8_t fma;
    uint8_t cx16;
    uint8_t xtpr;
    uint8_t pdcm;
    // reserved
    uint8_t pcid;
    uint8_t dca;
    uint8_t sse4_1;
    uint8_t sse4_2;
    uint8_t x2apic;
    uint8_t movbe;
    uint8_t popcnt;
    uint8_t tsc_deadline;
    uint8_t aes_ni;
    uint8_t xsave;
    uint8_t osxsave;
    uint8_t avx;
    uint8_t f16c;
    uint8_t rdrnd;
    uint8_t hypervisor;
} cpuid_feat_t;

typedef struct {
    // EDX
    uint8_t fpu;
    uint8_t vme;
    uint8_t de;
    uint8_t pse;
    uint8_t tsc;
    uint8_t msr;
    uint8_t pae;
    uint8_t mce;
    uint8_t cx8;
    uint8_t apic;
    uint8_t syscall_k6;
    uint8_t syscall;
    uint8_t mtrr;
    uint8_t pge;
    uint8_t mca;
    uint8_t cmov;
    uint8_t pat;
    uint8_t pse36;
    uint8_t ecc_k7;
    uint8_t ecc;
    uint8_t nx;
    uint8_t sem;
    uint8_t mmxext;
    uint8_t mmx;
    uint8_t fxsr;
    uint8_t fxsr_opt;
    uint8_t pdpe1gb;
    uint8_t rdtscp;
    uint8_t rex32_k8;
    uint8_t lm;
    uint8_t tdnowext;
    uint8_t tdnow;
    // ECX
    uint8_t lahf_lm;
    uint8_t cmp_legacy;
    uint8_t svm;
    uint8_t extapic;
    uint8_t cr8_legacy;
    uint8_t abm;
    uint8_t sse4a;
    uint8_t misalignsse;
    uint8_t tdnowprefetch;
    uint8_t osvw;
    uint8_t ibs;
    uint8_t xop;
    uint8_t skinit;
    uint8_t wdt;
    uint8_t tbm0;
    uint8_t lwp;
    uint8_t fma4;
    uint8_t tce;
    uint8_t cvt16;
    uint8_t nodeid_msr;
    // reserved
    uint8_t tbm;
    uint8_t topoext;
    uint8_t perfctr_core;
    uint8_t perfctr_nb;
    uint8_t StreamPerfMon;
    uint8_t dbx;
    uint8_t perftsc;
    uint8_t pcx_l2i_l3;
    uint8_t monitorx;
    uint8_t addr_mask_ext;
    //reserved
} cpuid_ext_feat_t;

// Topology from leaf 0x0B (Intel extended topology).
typedef struct {
    uint8_t  level_type;            // 1=Thread, 2=Core, 3=Module, etc.
    uint32_t logical_per_level;
    uint32_t level_number;
    uint32_t x2apic_id;
    uint8_t  valid;
} cpuid_topology_t;

// Frequency info from leaves 0x15 and 0x16.
typedef struct {
    uint32_t tsc_denominator;
    uint32_t tsc_numerator;
    uint64_t tsc_freq_hz;
    uint32_t base_freq_mhz;         // Leaf 0x16 EAX
    uint32_t max_freq_mhz;          // Leaf 0x16 EBX
    uint32_t bus_freq_mhz;          // Leaf 0x16 ECX
} cpuid_freq_info_t;

// Main CPU info structure.
typedef struct {
    char     vendor[13];
    char     brand[49];

    uint32_t psn[3];

    uint32_t max_basic_leaf;
    uint32_t max_extended_leaf;

    uint32_t features_ecx;
    uint32_t features_edx;

    cpuid_proc_info_t   proc;
    cpuid_feat7_t       feat7;
    cpuid_addr_size_t   addr_size;
    cpuid_freq_info_t   freq;
    uint32_t            features_ext_ecx;
    uint32_t            features_ext_edx;
    cpuid_feat_t        features;
    cpuid_ext_feat_t    features_ext;

    // --- Embedded sub-structures ---
    cpuid_mwait_info_t  mwait;
    cpuid_thermal_info_t thermal;
    cpuid_cache_info_t  caches[8];      // Leaf 4: usually 4-6 caches
    uint32_t            cache_count;
    cpuid_topology_t    topo[8];        // Leaf 0x0B: usually 2-3 levels
    uint32_t            topo_count;
    cpuid_svm_info_t    svm;            // Leaf 0x8000000A

    uint8_t             has_mwait;
    uint8_t             has_thermal;
    uint8_t             has_svm;
    uint8_t             has_topology;
} cpu_info_t;

/* --- Globals ---*/

/* --- Prototypes ---*/
int      cpuid_available(void);
void     cpuid_get_vendor(char *vendor);
void     cpuid_get_brand(char *brand);
void     cpuid_get_features(uint32_t *ecx, uint32_t *edx);
void     cpuid_get_proc_info(cpuid_proc_info_t *info);
int      cpuid_get_cache_info(uint32_t index, cpuid_cache_info_t *info);
void     cpuid_get_mwait_info(cpuid_mwait_info_t *info);
void     cpuid_get_thermal_info(cpuid_thermal_info_t *info);
void     cpuid_get_feat7(cpuid_feat7_t *info);
void     cpuid_get_addr_size(cpuid_addr_size_t *info);
void     cpuid_get_svm_info(cpuid_svm_info_t *info);
int      cpuid_get_topology(uint32_t level, cpuid_topology_t *info);
void     cpuid_get_freq(uint32_t max_basic, cpuid_freq_info_t *info);
void     cpuid_init(cpu_info_t *info);
void     cpuid_dump(cpu_info_t *info);

#endif