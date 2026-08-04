#include <mm/heap.h>
#include <mm/pmm.h>
#include <sync/spinlock.h>
#include <screen/screen.h>
#include <screen/printk.h>
#include <stdint.h>
#include <stddef.h>
#include <arch/x86/interrupts.h>
#include <internal/phonon_consts.h>
#include <lib/string.h>
#include <internal/kscope.h>
#include <internal/kscope_nodes.h>

#define ALIGN16(x) (((x) + 15) & ~15)

typedef struct Block {
    size_t size;
    struct Block* next;
    int free;
} __attribute__((aligned(16))) Block;

#define BLOCK_SIZE sizeof(Block)
#define HEAP_SIZE           (32 * 1024 * 1024)
#define HEAP_INITIAL_PAGES  8
#define HEAP_ARENA_PAGES    (HEAP_SIZE / FRAME_SIZE)

uint8_t* heap_base;
uint8_t* heap_end;
uint8_t* heap_break;

static Block* head = NULL;
static spinlock_t heap_lock;

static int heap_init(void) {
    const boot_info_t *info = pmm_get_boot_info();
    if (!info || !info->valid) {
        printk("[heap] Boot info not available\n");
        return -1;
    }

    uintptr_t start = (info->kernel_end + FRAME_ALIGN - 1) & ~(FRAME_ALIGN - 1);

    if (pmm_is_region_free(start, HEAP_SIZE) != 0) {
        printk("[heap] heap region not free\n");
        panic("The heap is doing it again", 0xFFFF, 0xDEAD);
    }
    printk("[heap] heap region free\n");
    pmm_reserve_region(start, HEAP_SIZE);

    heap_base = (uint8_t *)start;
    heap_end  = heap_base + (HEAP_INITIAL_PAGES * FRAME_SIZE);
    heap_break = heap_base;

    spinlock_init(&heap_lock);

    printk("[heap] Initialized at %p, initial %u pages, total arena %u MB\n", heap_base, HEAP_INITIAL_PAGES, HEAP_SIZE / (1024*1024));
    return 0;
}

kscope_node_t heap_node = {
    .name = "heap",
    .id = 0x0008,
    .class = KSCOPE_CLASS_MEMORY,
    .subclass = KSCOPE_SUBCLASS_MEMORY_HEAP,
    .requires = (kscope_node_t *[]){&pmm_node, &paging_node},
    .require_count = 2,
    .provides = (const char *[]){"mem.heap", "mem.kmalloc"},
    .provide_count = 2,
    .init = heap_init,
};

void* malloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN16(size);

    uint32_t flags = spinlock_acquire(&heap_lock);

    if (!head) {
        void* allocated = sbrk(BLOCK_SIZE + size);
        if (allocated == (void*)-1) {
            spinlock_release(&heap_lock, flags);
            return NULL;
        }
        head = (Block*)heap_base;
        head->size = size;
        head->next = NULL;
        head->free = 0;
        spinlock_release(&heap_lock, flags);
        return (void*)(head + 1);
    }

    Block* current = head;
    while (current) {
        if (current->free && current->size >= size) {

            size_t leftover = current->size - size;
            if (leftover > BLOCK_SIZE + 8) {
                Block* new_block = (Block*)((uint8_t*)(current + 1) + size);
                new_block->size = leftover - BLOCK_SIZE;
                new_block->free = 1;
                new_block->next = current->next;

                current->size = size;
                current->free = 0;
                current->next = new_block;

                spinlock_release(&heap_lock, flags);
                return (void*)(current + 1);
            } else {
                current->free = 0;
                spinlock_release(&heap_lock, flags);
                return (void*)(current + 1);
            }
        }

        if (!current->next) break;
        current = current->next;
    }

    uint8_t* next_addr = (uint8_t*)current + BLOCK_SIZE + current->size;
    uint8_t* alloc = sbrk(BLOCK_SIZE + size);
    if (alloc == (void*)-1 || alloc != next_addr) {
        spinlock_release(&heap_lock, flags);
        return NULL;
    }

    Block* new_block = (Block*)next_addr;
    new_block->size = size;
    new_block->free = 0;
    new_block->next = NULL;
    current->next = new_block;

    spinlock_release(&heap_lock, flags);
    return (void*)(new_block + 1);
}

void free(void* ptr) {
    if (!ptr) return;
    if ((uintptr_t)ptr & 0xF) return;

    uint32_t flags = spinlock_acquire(&heap_lock);

    Block* block = (Block*)ptr - 1;
    if ((uint8_t*)block < heap_base || (uint8_t*)block >= heap_end) {
        spinlock_release(&heap_lock, flags);
        return;
    }

    if (block->free) {
        spinlock_release(&heap_lock, flags);
        return;
    }
    block->free = 1;

    Block* next = block->next;
    if (next && next->free) {
        block->size += BLOCK_SIZE + next->size;
        block->next = next->next;
    }

    Block* prev = head;
    while (prev && prev->next != block) {
        prev = prev->next;
    }
    if (prev && prev->free) {
        prev->size += BLOCK_SIZE + block->size;
        prev->next = block->next;
    }

    spinlock_release(&heap_lock, flags);
}

void* calloc(size_t num, size_t size) {
    if (num == 0 || size == 0) return NULL;
    if (SIZE_MAX / num < size) return NULL;
    size_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void* realloc(void* ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    uint32_t flags = spinlock_acquire(&heap_lock);

    Block* block = (Block*)ptr - 1;
    if (block->size >= new_size) {
        size_t leftover = block->size - new_size;
        if (leftover > BLOCK_SIZE + 16) {
            Block* new_block = (Block*)((uint8_t*)(block + 1) + new_size);
            new_block->size = leftover - BLOCK_SIZE;
            new_block->free = 1;
            new_block->next = block->next;

            block->size = new_size;
            block->next = new_block;

            if (new_block->next && new_block->next->free) {
                new_block->size += BLOCK_SIZE + new_block->next->size;
                new_block->next = new_block->next->next;
            }
        }
        spinlock_release(&heap_lock, flags);
        return ptr;
    }

    if (block->next && block->next->free) {
        size_t combined = block->size + BLOCK_SIZE + block->next->size;
        if (combined >= new_size) {
            block->size = combined;
            block->next = block->next->next;
            spinlock_release(&heap_lock, flags);
            return ptr;
        }
    }

    spinlock_release(&heap_lock, flags);

    /* Falls outside the lock: malloc/free below re-acquire on their own,
       and copying doesn't touch shared heap state */
    void* new_ptr = malloc(new_size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

void print_block(Block* b) {
    puts("Block @ ");
    puthex((uint32_t)b);
    puts(", size=");
    char buf[12];
    int_to_ascii((int)b->size, buf);
    puts(buf);
    puts(", free=");
    puts(b->free ? "yes" : "no");
    puts("\n");
}

void print_heap_state() {
    Block* current = head;
    puts("Heap blocks:\n");
    while (current) {
        print_block(current);
        current = current->next;
    }
}

void* sbrk(ptrdiff_t increment) {
    uint32_t flags = spinlock_acquire(&heap_lock);

    uint8_t* prev_break = heap_break;
    uint8_t* new_break = heap_break + increment;

    if (increment == 0) {
        spinlock_release(&heap_lock, flags);
        return prev_break;
    }

    if (new_break < heap_base) {
        printk("[heap] sbrk: cannot shrink below heap base\n");
        spinlock_release(&heap_lock, flags);
        return (void*)-1;
    }

    if (new_break > heap_base + HEAP_SIZE) {
        printk("[heap] sbrk: heap arena exhausted (need %p, limit %p)\n", new_break, heap_base + HEAP_SIZE);
        spinlock_release(&heap_lock, flags);
        return (void*)-1;
    }

    if (new_break > heap_end) {
        uint32_t needed = (uint32_t)(new_break - heap_end);
        uint32_t frames = (needed + FRAME_SIZE - 1) >> FRAME_SIZE_SHIFT;
        if (frames == 0) frames = 1;

        printk("[heap] sbrk: growing committed region %p -> %p (+%u frames)\n", heap_end, heap_end + frames * FRAME_SIZE, frames);
        heap_end += frames * FRAME_SIZE;
    }

    heap_break = new_break;
    spinlock_release(&heap_lock, flags);
    return prev_break;
}