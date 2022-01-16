#include <j6/errors.h>
#include <j6/types.h>

#include "cpu.h"
#include "device_manager.h"
#include "frame_allocator.h"
#include "logger.h"
#include "memory.h"
#include "objects/endpoint.h"
#include "objects/thread.h"
#include "objects/system.h"
#include "objects/vm_area.h"
#include "syscalls/helpers.h"

extern log::logger &g_logger;

namespace syscalls {

j6_status_t
log(const char *message)
{
    thread &th = thread::current();
    log::info(logs::syscall, "Message[%llx]: %s", th.koid(), message);
    return j6_status_ok;
}

j6_status_t
noop()
{
    thread &th = thread::current();
    log::debug(logs::syscall, "Thread %llx called noop syscall.", th.koid());
    return j6_status_ok;
}

j6_status_t
system_get_log(j6_handle_t sys, void *buffer, size_t *buffer_len)
{
    // Buffer is marked optional, but we need the length, and if length > 0,
    // buffer is not optional.
    if (!buffer_len || (*buffer_len && !buffer))
        return j6_err_invalid_arg;

    size_t orig_size = *buffer_len;
    *buffer_len = g_logger.get_entry(buffer, *buffer_len);
    if (!g_logger.has_log())
        system::get().deassert_signal(j6_signal_system_has_log);

    return (*buffer_len > orig_size) ? j6_err_insufficient : j6_status_ok;
}

j6_status_t
system_bind_irq(j6_handle_t sys, j6_handle_t endp, unsigned irq)
{
    endpoint *e = get_handle<endpoint>(endp);
    if (!e) return j6_err_invalid_arg;

    if (device_manager::get().bind_irq(irq, e))
        return j6_status_ok;

    return j6_err_invalid_arg;
}

j6_status_t
system_map_phys(j6_handle_t handle, j6_handle_t * area, uintptr_t phys, size_t size, uint32_t flags)
{
    // TODO: check to see if frames are already used? How would that collide with
    // the bootloader's allocated pages already being marked used?
    if (!(flags & vm_flags::mmio))
        frame_allocator::get().used(phys, mem::page_count(size));

    vm_flags vmf = (static_cast<vm_flags>(flags) & vm_flags::driver_mask);
    construct_handle<vm_area_fixed>(area, phys, size, vmf);

    return j6_status_ok;
}

j6_status_t
system_request_iopl(j6_handle_t handle, unsigned iopl)
{
    if (iopl != 0 && iopl != 3)
        return j6_err_invalid_arg;

    constexpr uint64_t mask = 3 << 12;
    cpu_data &cpu = current_cpu();
    cpu.rflags3 = (cpu.rflags3 & ~mask) | ((iopl << 12) & mask);
    return j6_status_ok;
}

} // namespace syscalls
