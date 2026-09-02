/*
 * mm/paging.c - x86 paging (i386 2-level / x86_64 4-level)
 * Author:   amity
 * Date:     Mon Jun 22 10:45:00 2026
 * Copyright © 2026 OwlyNest
 *
 */

/* --- Styling Instructions ---
 * Encoding:                      UTF-8, Unix line endings
 * Text font:                     Monospace
 * Line width:                    Max 80 characters
 * Indentation:                   Use 4 spaces
 * Brace style:                   Same line as control statement
 * Inline comments:               Column 40, wherever possible, else, whole
 * multiple of 20 Section headers:               Use 3 '-' characters before and
 * after Pointer notation:              Next to variable name, not type Binary
 * operations:             Space around operator Empty parameter list: Use
 * (void) instead of () Statements and declarations:   Max one per line
 */

/*
 * The public API in paging.h is architecture-agnostic. The walk,
 * recursive mapping, and CR3 load are not — they live behind
 * ARCH_X86_64 / ARCH_X86_32 in this file. Prefer splitting into
 * arch/x86/{32,64}/paging.c once the 64-bit tree is wired; the
 * ifdefs are a drop-in so `make ARCH=x64` can start compiling.
 *
 * Long-mode note: CR0.PG is already set by the time this runs
 * (you cannot be in 64-bit code without paging). paging_init()
 * therefore builds a new PML4 and switches CR3; it does not
 * touch CR0. The bootstrap identity map must still cover frames
 * returned by pmm_alloc_frame() so we can memset new tables
 * before the recursive mapping exists.
 */

#include <internal/kscope.h>
#include <internal/kscope_nodes.h>
#include <internal/phonon_types.h>
#include <internal/virtmem.h>
#include <lib/string.h>
#include <mm/mmap.h>
#include <mm/paging.h>
#include <mm/pmm.h>
#include <screen/printk.h>
#include <stdint.h>
#include <sync/spinlock.h>

static PHYS_ADDR_T root_phys = 0;
static SIZE_T identity_map_size = 0;
static _SPINLOCK paging_lock;

extern uint8_t __kernel_phys_start[];
extern uint8_t _kernel_phys_end[];

FORCEINLINE void paging_invlpg(VIRT_ADDR_T virt) {
  __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
}

FORCEINLINE void paging_load_cr3(PHYS_ADDR_T phys) {
  __asm__ __volatile__("mov %0, %%cr3" : : "r"(phys) : "memory");
}

/* ==========================================================================
 * Shared kscope node
 * ======================================================================= */

static kscope_node_t *paging_requires[] = {&pmm_node};
static const char *paging_provides[] = {"mem.paging"};

SIZE_T paging_get_identity_size(void) { return identity_map_size; }

#if ARCH_X86_64

/* ==========================================================================
 * x86_64 — 4-level, recursive PML4[RECURSIVE_PML4_INDEX]
 * ======================================================================= */

#define IDX_PML4(v) ((ULONG)(((VIRT_ADDR_T)(v) >> 39) & 0x1FFu))
#define IDX_PDPT(v) ((ULONG)(((VIRT_ADDR_T)(v) >> 30) & 0x1FFu))
#define IDX_PD(v) ((ULONG)(((VIRT_ADDR_T)(v) >> 21) & 0x1FFu))
#define IDX_PT(v) ((ULONG)(((VIRT_ADDR_T)(v) >> 12) & 0x1FFu))

#define HUGE_2M (2u * 1024u * 1024u)

static PTE_T *phys_as_ptr(PHYS_ADDR_T p) { return (PTE_T *)(VIRT_ADDR_T)p; }

static int boot_ensure(PTE_T *slot) {
  PHYS_ADDR_T frame;

  if (*slot & PAGE_PRESENT)
    return 0;

  frame = pmm_alloc_frame();
  if (!frame)
    return -1;
  memset(phys_as_ptr(frame), 0, PAGE_SIZE);
  *slot = (PTE_T)frame | PAGE_PRESENT | PAGE_WRITABLE;
  return 0;
}

static int boot_map_2m(PTE_T *pml4, PHYS_ADDR_T phys, VIRT_ADDR_T virt,
                       ULONG64 flags) {
  ULONG i4 = IDX_PML4(virt);
  ULONG i3 = IDX_PDPT(virt);
  ULONG i2 = IDX_PD(virt);
  PTE_T *pdpt;
  PTE_T *pd;

  if (boot_ensure(&pml4[i4]) != 0)
    return -1;
  pdpt = phys_as_ptr(PTE_ADDR(pml4[i4]));

  if (boot_ensure(&pdpt[i3]) != 0)
    return -1;
  pd = phys_as_ptr(PTE_ADDR(pdpt[i3]));

  pd[i2] = (PTE_T)(phys & ~((PHYS_ADDR_T)HUGE_2M - 1)) | PAGE_PRESENT |
           PAGE_HUGE | (PTE_T)(flags & PAGE_FLAG_MASK);
  return 0;
}

static int boot_map_4k(PTE_T *pml4, PHYS_ADDR_T phys, VIRT_ADDR_T virt,
                       ULONG64 flags) {
  ULONG i4 = IDX_PML4(virt);
  ULONG i3 = IDX_PDPT(virt);
  ULONG i2 = IDX_PD(virt);
  ULONG i1 = IDX_PT(virt);
  PTE_T *pdpt;
  PTE_T *pd;
  PTE_T *pt;

  if (boot_ensure(&pml4[i4]) != 0)
    return -1;
  pdpt = phys_as_ptr(PTE_ADDR(pml4[i4]));

  if (boot_ensure(&pdpt[i3]) != 0)
    return -1;
  pd = phys_as_ptr(PTE_ADDR(pdpt[i3]));

  if (pd[i2] & PAGE_HUGE)
    return -1;
  if (boot_ensure(&pd[i2]) != 0)
    return -1;
  pt = phys_as_ptr(PTE_ADDR(pd[i2]));

  pt[i1] =
      (PTE_T)PTE_ADDR(phys) | PAGE_PRESENT | (PTE_T)(flags & PAGE_FLAG_MASK);
  return 0;
}

static int boot_map_range_2m(PTE_T *pml4, PHYS_ADDR_T phys, VIRT_ADDR_T virt,
                             SIZE_T length, ULONG64 flags) {
  PHYS_ADDR_T p = phys & ~((PHYS_ADDR_T)HUGE_2M - 1);
  VIRT_ADDR_T v = virt & ~((VIRT_ADDR_T)HUGE_2M - 1);
  VIRT_ADDR_T end = (virt + length + HUGE_2M - 1) & ~((VIRT_ADDR_T)HUGE_2M - 1);

  while (v < end) {
    if (boot_map_2m(pml4, p, v, flags) != 0)
      return -1;
    p += HUGE_2M;
    v += HUGE_2M;
  }
  return 0;
}

int paging_init(void) {
  PTE_T *pml4;
  ULONGLONG ram;
  SIZE_T ident;
  PHYS_ADDR_T phys_start;
  PHYS_ADDR_T phys_end;
  PHYS_ADDR_T p;
  PHYS_ADDR_T paging_end;
  PHYS_ADDR_T kernel_phys_end;

  spinlock_init(&paging_lock);

  root_phys = pmm_alloc_frame();
  if (!root_phys)
    return -1;
  pml4 = phys_as_ptr(root_phys);
  memset(pml4, 0, PAGE_SIZE);

  ram = pmm_get_total_ram();

  /* Bootstrap identity window: first 4 GiB or all RAM, whichever
   * is smaller. Keeps auto_virt() and early physical pointers
   * working. 2 MiB pages. */
  ident = (SIZE_T)ram;
  if (ident > 0x100000000ULL)
    ident = (SIZE_T)0x100000000ULL;
  ident &= ~((SIZE_T)HUGE_2M - 1);
  if (ident < HUGE_2M)
    ident = HUGE_2M;

  if (boot_map_range_2m(pml4, 0, 0, ident, PAGE_WRITABLE) != 0)
    return -1;
  identity_map_size = ident;

  /* Direct map of all physical RAM (capped by DIRECT_MAP_SIZE). */
  {
    SIZE_T dlen = (SIZE_T)ram;
    if (dlen > (SIZE_T)DIRECT_MAP_SIZE)
      dlen = (SIZE_T)DIRECT_MAP_SIZE;
    dlen = (dlen + HUGE_2M - 1) & ~((SIZE_T)HUGE_2M - 1);
    if (boot_map_range_2m(pml4, 0, DIRECT_MAP_BASE, dlen,
                          PAGE_WRITABLE | PAGE_NX) != 0)
      return -1;
  }

  /* Higher-half kernel image at KERNEL_VIRT_BASE + phys. */
  phys_start = (PHYS_ADDR_T)(ULONG_PTR)__kernel_phys_start;
  phys_end = ((PHYS_ADDR_T)(ULONG_PTR)_kernel_phys_end + PAGE_MASK) &
             ~(PHYS_ADDR_T)PAGE_MASK;

  for (p = phys_start; p < phys_end; p += PAGE_SIZE) {
    VIRT_ADDR_T v = KERNEL_VIRT_BASE + (VIRT_ADDR_T)p;
    if (boot_map_4k(pml4, p, v, PAGE_WRITABLE) != 0)
      return -1;
  }

  /* Recursive mapping. Must not be PML4[511] — that slot is the
   * kernel image. */
  pml4[RECURSIVE_PML4_INDEX] = (PTE_T)root_phys | PAGE_PRESENT | PAGE_WRITABLE;

  paging_load_cr3(root_phys);

  /* Immediate verification that does not need printk or the IDT. */
  {
    volatile PTE_T *rec = (volatile PTE_T *)RECURSIVE_PML4_BASE;
    if ((rec[RECURSIVE_PML4_INDEX] & PAGE_PRESENT) == 0) {
      __asm__ __volatile__("mov $0xDEAD5EC0, %%rax; hlt" ::: "rax");
    }
  }

  kernel_phys_end = ((PHYS_ADDR_T)(ULONG_PTR)_kernel_phys_end + PAGE_MASK) &
                    ~(PHYS_ADDR_T)PAGE_MASK;
  paging_end = root_phys + PAGE_SIZE;
  if (paging_end < kernel_phys_end)
    paging_end = kernel_phys_end;
  pmm_set_kernel_end((paging_end + FRAME_ALIGN - 1) &
                     ~(PHYS_ADDR_T)FRAME_ALIGN);

  printk("[paging] long mode, PML4=%p, identity %llu MB, "
         "direct map %p\n",
         (PVOID)(VIRT_ADDR_T)root_phys,
         (unsigned long long)(identity_map_size >> 20),
         (PVOID)(VIRT_ADDR_T)DIRECT_MAP_BASE);
  return 0;
}

static int ensure_table(PTE_T *slot, VIRT_ADDR_T table_virt) {
  PHYS_ADDR_T frame;

  if (*slot & PAGE_PRESENT) {
    if (*slot & PAGE_HUGE)
      return -1;
    return 0;
  }

  frame = pmm_alloc_frame();
  if (!frame)
    return -1;

  *slot = (PTE_T)frame | PAGE_PRESENT | PAGE_WRITABLE;
  paging_invlpg(table_virt);
  memset((PVOID)table_virt, 0, PAGE_SIZE);
  return 0;
}

int map_page(PHYS_ADDR_T phys, VIRT_ADDR_T virt, ULONG64 flags) {
  ULONG i4, i3, i2, i1;
  ULONG saved;
  PTE_T *pt;

  if (phys & PAGE_MASK) {
    printk("[paging] map_page: phys %p not aligned\n",
           (PVOID)(VIRT_ADDR_T)phys);
    return -1;
  }
  if (virt & PAGE_MASK) {
    printk("[paging] map_page: virt %p not aligned\n", (PVOID)virt);
    return -1;
  }

  i4 = IDX_PML4(virt);
  i3 = IDX_PDPT(virt);
  i2 = IDX_PD(virt);
  i1 = IDX_PT(virt);

  saved = spinlock_acquire(&paging_lock);

  if (ensure_table(&PML4_VIRT[i4], (VIRT_ADDR_T)PDPT_VIRT(i4)) != 0) {
    spinlock_release(&paging_lock, saved);
    return -1;
  }
  if (ensure_table(&PDPT_VIRT(i4)[i3], (VIRT_ADDR_T)PD_VIRT_AT(i4, i3)) != 0) {
    spinlock_release(&paging_lock, saved);
    return -1;
  }
  if (ensure_table(&PD_VIRT_AT(i4, i3)[i2],
                   (VIRT_ADDR_T)PT_VIRT_AT(i4, i3, i2)) != 0) {
    spinlock_release(&paging_lock, saved);
    return -1;
  }

  pt = PT_VIRT_AT(i4, i3, i2);
  pt[i1] =
      (PTE_T)PTE_ADDR(phys) | PAGE_PRESENT | (PTE_T)(flags & PAGE_FLAG_MASK);

  paging_invlpg(virt);
  spinlock_release(&paging_lock, saved);
  return 0;
}

void unmap_page(VIRT_ADDR_T virt) {
  ULONG i4 = IDX_PML4(virt);
  ULONG i3 = IDX_PDPT(virt);
  ULONG i2 = IDX_PD(virt);
  ULONG i1 = IDX_PT(virt);
  ULONG saved = spinlock_acquire(&paging_lock);

  if (!(PML4_VIRT[i4] & PAGE_PRESENT) || !(PDPT_VIRT(i4)[i3] & PAGE_PRESENT) ||
      !(PD_VIRT_AT(i4, i3)[i2] & PAGE_PRESENT) ||
      (PD_VIRT_AT(i4, i3)[i2] & PAGE_HUGE)) {
    spinlock_release(&paging_lock, saved);
    return;
  }

  PT_VIRT_AT(i4, i3, i2)[i1] = 0;
  paging_invlpg(virt);
  spinlock_release(&paging_lock, saved);
}

PHYS_ADDR_T virt_to_phys(VIRT_ADDR_T virt) {
  ULONG i4 = IDX_PML4(virt);
  ULONG i3 = IDX_PDPT(virt);
  ULONG i2 = IDX_PD(virt);
  ULONG i1 = IDX_PT(virt);
  PTE_T e;

  if (!(PML4_VIRT[i4] & PAGE_PRESENT))
    return PHYS_ADDR_INVALID;
  if (!(PDPT_VIRT(i4)[i3] & PAGE_PRESENT))
    return PHYS_ADDR_INVALID;

  e = PD_VIRT_AT(i4, i3)[i2];
  if (!(e & PAGE_PRESENT))
    return PHYS_ADDR_INVALID;

  if (e & PAGE_HUGE) {
    return PTE_ADDR(e) | (PHYS_ADDR_T)(virt & (HUGE_2M - 1));
  }

  e = PT_VIRT_AT(i4, i3, i2)[i1];
  if (!(e & PAGE_PRESENT))
    return PHYS_ADDR_INVALID;

  return PTE_ADDR(e) | (PHYS_ADDR_T)(virt & PAGE_MASK);
}

#else /* ARCH_X86_32 */

/* ==========================================================================
 * i386 — 2-level, recursive PD[1023]
 * ======================================================================= */

int paging_init(void) {
  PTE_T *pd;
  SIZE_T pts_needed;
  ULONG pd_idx;
  PHYS_ADDR_T phys_start;
  PHYS_ADDR_T phys_end;
  PHYS_ADDR_T p;
  PHYS_ADDR_T kernel_phys_end;
  PHYS_ADDR_T paging_end;
  ULONG cr0;

  spinlock_init(&paging_lock);

  root_phys = pmm_alloc_frame();
  if (!root_phys)
    return -1;
  pd = (PTE_T *)(VIRT_ADDR_T)root_phys;
  memset(pd, 0, PAGE_SIZE);

  /* Identity-map as much RAM as we have, but never touch slot 1023 */
  pts_needed = ((SIZE_T)(pmm_get_total_ram() >> 22)) + 1;
  if (pts_needed > 1023)
    pts_needed = 1023;
  identity_map_size = pts_needed * 4u * 1024u * 1024u;

  for (pd_idx = 0; pd_idx < (ULONG)pts_needed; pd_idx++) {
    PHYS_ADDR_T pt_phys = pmm_alloc_frame();
    PTE_T *pt;
    int j;

    if (!pt_phys)
      return -1;
    pt = (PTE_T *)(VIRT_ADDR_T)pt_phys;
    memset(pt, 0, PAGE_SIZE);

    for (j = 0; j < PT_ENTRIES; j++) {
      PHYS_ADDR_T phys =
          ((PHYS_ADDR_T)pd_idx * PT_ENTRIES + (PHYS_ADDR_T)j) * PAGE_SIZE;
      pt[j] = (PTE_T)phys | PAGE_PRESENT | PAGE_WRITABLE;
    }
    pd[pd_idx] = (PTE_T)pt_phys | PAGE_PRESENT | PAGE_WRITABLE;
  }

  /* Higher-half mapping of the kernel image – done on the physical
   * PD/PTs before recursive mapping and before CR3 is loaded. */
  phys_start = (PHYS_ADDR_T)(ULONG_PTR)__kernel_phys_start;
  phys_end = ((PHYS_ADDR_T)(ULONG_PTR)_kernel_phys_end + PAGE_MASK) &
             ~(PHYS_ADDR_T)PAGE_MASK;

  for (p = phys_start; p < phys_end; p += PAGE_SIZE) {
    VIRT_ADDR_T v = (VIRT_ADDR_T)p + KERNEL_VIRT_BASE;
    ULONG pd_i = (ULONG)(v >> 22);
    ULONG pt_i = (ULONG)((v >> 12) & 0x3FFu);
    PTE_T *pt;

    if (!(pd[pd_i] & PAGE_PRESENT)) {
      PHYS_ADDR_T pt_phys = pmm_alloc_frame();
      if (!pt_phys)
        return -1;
      memset((PVOID)(VIRT_ADDR_T)pt_phys, 0, PAGE_SIZE);
      pd[pd_i] = (PTE_T)pt_phys | PAGE_PRESENT | PAGE_WRITABLE;
    }

    pt = (PTE_T *)(VIRT_ADDR_T)PTE_ADDR(pd[pd_i]);
    pt[pt_i] = (PTE_T)p | PAGE_PRESENT | PAGE_WRITABLE;
  }

  pd[1023] = (PTE_T)root_phys | PAGE_PRESENT | PAGE_WRITABLE;
  paging_load_cr3(root_phys);

  __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80000000u;
  __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");

  {
    volatile PTE_T *rec = (volatile PTE_T *)RECURSIVE_PD_BASE;
    if ((rec[1023] & PAGE_PRESENT) == 0) {
      __asm__ __volatile__("mov $0xDEAD5EC0, %%eax; hlt" ::: "eax");
    }
  }

  kernel_phys_end = ((PHYS_ADDR_T)(ULONG_PTR)_kernel_phys_end + PAGE_MASK) &
                    ~(PHYS_ADDR_T)PAGE_MASK;
  paging_end = root_phys + (PHYS_ADDR_T)(pts_needed + 1 + 4) * PAGE_SIZE;
  if (paging_end < kernel_phys_end)
    paging_end = kernel_phys_end;
  pmm_set_kernel_end((paging_end + FRAME_ALIGN - 1) &
                     ~(PHYS_ADDR_T)FRAME_ALIGN);

  return 0;
}

int map_page(PHYS_ADDR_T phys, VIRT_ADDR_T virt, ULONG64 flags) {
  ULONG pd_idx;
  ULONG pt_idx;
  ULONG saved;

  if (phys & PAGE_MASK) {
    printk("[paging] map_page: phys 0x%08x not aligned\n", (ULONG)phys);
    return -1;
  }
  if (virt & PAGE_MASK) {
    printk("[paging] map_page: virt 0x%08x not aligned\n", (ULONG)virt);
    return -1;
  }

  pd_idx = (ULONG)(virt >> 22);
  pt_idx = (ULONG)((virt >> 12) & 0x3FFu);

  saved = spinlock_acquire(&paging_lock);

  if (!(PD_VIRT[pd_idx] & PAGE_PRESENT)) {
    PHYS_ADDR_T new_pt_phys = pmm_alloc_frame();
    VIRT_ADDR_T pt_virt_addr;

    if (!new_pt_phys) {
      spinlock_release(&paging_lock, saved);
      return -1;
    }

    PD_VIRT[pd_idx] = (PTE_T)new_pt_phys | PAGE_PRESENT | PAGE_WRITABLE;

    pt_virt_addr = RECURSIVE_PT_BASE + ((VIRT_ADDR_T)pd_idx * PAGE_SIZE);
    paging_invlpg(pt_virt_addr);

    memset(PT_VIRT(pd_idx), 0, PAGE_SIZE);
  }

  PT_VIRT(pd_idx)
  [pt_idx] =
      (PTE_T)PTE_ADDR(phys) | PAGE_PRESENT | (PTE_T)(flags & PAGE_FLAG_MASK);

  paging_invlpg(virt);
  spinlock_release(&paging_lock, saved);
  return 0;
}

void unmap_page(VIRT_ADDR_T virt) {
  ULONG pd_idx = (ULONG)(virt >> 22);
  ULONG pt_idx = (ULONG)((virt >> 12) & 0x3FFu);
  ULONG saved = spinlock_acquire(&paging_lock);

  if (!(PD_VIRT[pd_idx] & PAGE_PRESENT)) {
    spinlock_release(&paging_lock, saved);
    return;
  }

  PT_VIRT(pd_idx)[pt_idx] = 0;
  paging_invlpg(virt);
  spinlock_release(&paging_lock, saved);
}

PHYS_ADDR_T virt_to_phys(VIRT_ADDR_T virt) {
  ULONG pd_idx = (ULONG)(virt >> 22);
  ULONG pt_idx = (ULONG)((virt >> 12) & 0x3FFu);

  if (!(PD_VIRT[pd_idx] & PAGE_PRESENT))
    return PHYS_ADDR_INVALID;

  if (!(PT_VIRT(pd_idx)[pt_idx] & PAGE_PRESENT))
    return PHYS_ADDR_INVALID;

  return PTE_ADDR(PT_VIRT(pd_idx)[pt_idx]) | (PHYS_ADDR_T)(virt & PAGE_MASK);
}

#endif /* ARCH_X86_32 */

kscope_node_t paging_node = {.name = "paging",
                             .id = 0x13,
                             .class = KSCOPE_CLASS_MEMORY,
                             .subclass = KSCOPE_SUBCLASS_MEMORY_PAGING,
                             .requires = paging_requires,
                             .require_count = 1,
                             .provides = paging_provides,
                             .provide_count = 1,
                             .init = paging_init};