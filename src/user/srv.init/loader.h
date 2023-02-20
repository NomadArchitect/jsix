#pragma once
/// \file loader.h
/// Routines for loading and starting other programs

#include <j6/types.h>
#include <j6/flags.h>
#include <util/counted.h>

namespace bootproto {
    struct module;
}

bool load_program(
        const char *name,
        util::const_buffer data,
        j6_handle_t sys, j6_handle_t slp,
        const bootproto::module *arg = nullptr);

j6_handle_t map_phys(j6_handle_t sys, uintptr_t phys, size_t len, j6_vm_flags flags = j6_vm_flag_none);

inline j6_handle_t map_phys(j6_handle_t sys, const void *phys, size_t len, j6_vm_flags flags = j6_vm_flag_none) {
    return map_phys(sys, reinterpret_cast<uintptr_t>(phys), len, flags);
}
