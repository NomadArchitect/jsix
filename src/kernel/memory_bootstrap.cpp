#include <arch/memory.h>
#include <bootproto/kernel.h>
#include <util/no_construct.h>

#include "kassert.h"
#include "capabilities.h"
#include "device_manager.h"
#include "frame_allocator.h"
#include "heap_allocator.h"
#include "logger.h"
#include "memory.h"
#include "objects/process.h"
#include "objects/vm_area.h"
#include "vm_space.h"

extern "C" {
    void (*__ctors)(void);
    void (*__ctors_end)(void);
}

using bootproto::allocation_register;

inline constexpr util::bitset32 vm_flag_write = util::bitset32::of(obj::vm_flags::write);
inline constexpr util::bitset32 vm_flag_exact = util::bitset32::of(obj::vm_flags::exact);

// These objects are initialized _before_ global constructors are called,
// so we don't want them to have global constructors at all, lest they
// overwrite the previous initialization.
static util::no_construct<heap_allocator> __g_kernel_heap_storage;
heap_allocator &g_kernel_heap = __g_kernel_heap_storage.value;

static util::no_construct<cap_table> __g_cap_table_storage;
cap_table &g_cap_table = __g_cap_table_storage.value;

static util::no_construct<frame_allocator> __g_frame_allocator_storage;
frame_allocator &g_frame_allocator = __g_frame_allocator_storage.value;

static util::no_construct<obj::vm_area_untracked> __g_kernel_heap_area_storage;
obj::vm_area_untracked &g_kernel_heap_area = __g_kernel_heap_area_storage.value;

static util::no_construct<obj::vm_area_ring> __g_kernel_log_area_storage;
obj::vm_area_ring &g_kernel_log_area = __g_kernel_log_area_storage.value;

static util::no_construct<obj::vm_area_untracked> __g_kernel_heapmap_area_storage;
obj::vm_area_untracked &g_kernel_heapmap_area = __g_kernel_heapmap_area_storage.value;

static util::no_construct<obj::vm_area_untracked> __g_cap_table_area_storage;
obj::vm_area_untracked &g_cap_table_area = __g_cap_table_area_storage.value;

static util::no_construct<obj::vm_area_guarded> __g_kernel_stacks_storage;
obj::vm_area_guarded &g_kernel_stacks = __g_kernel_stacks_storage.value;

obj::vm_area_guarded g_kernel_buffers {
    mem::buffers_offset,
    mem::kernel_buffer_pages,
    mem::buffers_size,
    vm_flag_write};

void * operator new(size_t size)           { return g_kernel_heap.allocate(size); }
void * operator new [] (size_t size)       { return g_kernel_heap.allocate(size); }
void operator delete (void *p) noexcept    { return g_kernel_heap.free(p); }
void operator delete [] (void *p) noexcept { return g_kernel_heap.free(p); }

void * kalloc(size_t size) { return g_kernel_heap.allocate(size); }
void kfree(void *p) { return g_kernel_heap.free(p); }

template <typename T>
uintptr_t
get_physical_page(T *p) {
    return mem::page_align_down(reinterpret_cast<uintptr_t>(p));
}

namespace {

void
memory_initialize_pre_ctors(bootproto::args &kargs)
{
    using bootproto::frame_block;

    page_table *kpml4 = static_cast<page_table*>(kargs.pml4);

    // Initialize the frame allocator
    frame_block *blocks = reinterpret_cast<frame_block*>(mem::bitmap_offset);
    new (&g_frame_allocator) frame_allocator {blocks, kargs.frame_blocks.count};

    // Mark all the things the bootloader allocated for us as used
    allocation_register *reg = kargs.allocations;
    while (reg) {
        for (auto &alloc : reg->entries)
            if (alloc.type != bootproto::allocation_type::none)
                g_frame_allocator.used(alloc.address, alloc.count);
        reg = reg->next;
    }

    // Initialize the kernel "process" and vm_space
    obj::process *kp = obj::process::create_kernel_process(kpml4);
    vm_space &vm = kp->space();

    // Create the heap space and heap allocator
    obj::vm_area *heap = new (&g_kernel_heap_area)
        obj::vm_area_untracked(mem::heap_size, vm_flag_write);

    obj::vm_area *heap_map = new (&g_kernel_heapmap_area)
        obj::vm_area_untracked(mem::heapmap_size, vm_flag_write);

    vm.add(mem::heap_offset, heap, vm_flag_exact);
    vm.add(mem::heapmap_offset, heap_map, vm_flag_exact);

    new (&g_kernel_heap) heap_allocator {mem::heap_offset, mem::heap_size, mem::heapmap_offset};

    // Set up the log area and logger
    size_t log_buffer_size = log::log_pages * arch::frame_size;
    obj::vm_area *logs = new (&g_kernel_log_area)
        obj::vm_area_ring(log_buffer_size, vm_flag_write);
    vm.add(mem::logs_offset, logs, vm_flag_exact);

    new (&g_logger) log::logger(
            util::buffer::from(mem::logs_offset, log_buffer_size));

    // Set up the capability tables
    obj::vm_area *caps = new (&g_cap_table_area)
        obj::vm_area_untracked(mem::caps_size, vm_flag_write);

    vm.add(mem::caps_offset, caps, vm_flag_exact);

    new (&g_cap_table) cap_table {mem::caps_offset};

    obj::vm_area *stacks = new (&g_kernel_stacks) obj::vm_area_guarded {
        mem::stacks_offset,
        mem::kernel_stack_pages,
        mem::stacks_size,
        vm_flag_write};
    vm.add(mem::stacks_offset, &g_kernel_stacks, vm_flag_exact);

    // Clean out any remaning bootloader page table entries
    for (unsigned i = 0; i < arch::kernel_root_index; ++i)
        kpml4->entries[i] = 0;
}

void
memory_initialize_post_ctors(bootproto::args &kargs)
{
    vm_space &vm = vm_space::kernel_space();
    vm.add(mem::buffers_offset, &g_kernel_buffers, vm_flag_exact);

    g_frame_allocator.free(
        get_physical_page(kargs.page_tables.pointer),
        kargs.page_tables.count);
}

void
run_constructors()
{
    void (**p)(void) = &__ctors;
    while (p < &__ctors_end) {
        void (*ctor)(void) = *p++;
        if (ctor) ctor();
    }
}

} // namespace

namespace mem {

void
initialize(bootproto::args &args)
{
    memory_initialize_pre_ctors(args);
    run_constructors();
    memory_initialize_post_ctors(args);
}

} // namespace mem
