#pragma once
/// \file types.h
/// Basic kernel types exposed to userspace

#include <stdint.h>

/// All interactable kernel objects have a uniqe kernel object id
typedef uint64_t j6_koid_t;

/// Syscalls return status as this type
typedef uint64_t j6_status_t;

/// Some objects have signals, which are a bitmap of 64 possible signals
typedef uint64_t j6_signal_t;

/// The first word of IPC messages are the tag. Tags with the high bit
/// set are reserved for the system.
typedef uint64_t j6_tag_t;

#define j6_tag_system_flag    0x8000000000000000
#define j6_tag_invalid        0x0000000000000000

/// If all high bits except the last 16 are set, then the tag represents
/// an IRQ.
#define j6_tag_irq_base       0xffffffffffff0000
#define j6_tag_is_irq(x)      (((x) & j6_tag_irq_base) == j6_tag_irq_base)
#define j6_tag_from_irq(x)    ((x) | j6_tag_irq_base)
#define j6_tag_to_irq(x)      ((x) & ~j6_tag_irq_base)

/// Handles are references and capabilities to other objects. A handle is
/// an id in the lower 32 bits, a bitfield of capabilities in bits 32-55
/// and a type id in bits 56-63.
typedef uint64_t j6_handle_t;

/// Bitfield for storage of capabilities on their own
typedef uint32_t j6_cap_t;

#define j6_handle_invalid ((j6_handle_t)-1)

enum j6_object_type {
#define OBJECT_TYPE( name, val ) j6_object_type_ ## name = val,
#include <j6/tables/object_types.inc>
#undef OBJECT_TYPE

    j6_object_type_max
};
