// The kernel depends on libj6 for some shared code,
// but should not include the user-specific code.
#ifndef __j6kernel

#include <stdint.h>
#include <j6/init.h>
#include <j6/types.h>

static size_t __initc = 0;
static struct j6_init_value *__initv = 0;

j6_handle_t __handle_sys = j6_handle_invalid;
j6_handle_t __handle_self = j6_handle_invalid;

extern "C" void
_get_init(size_t *initc, struct j6_init_value **initv)
{
    if (!initc)
        return;

    *initc = __initc;
    if (initv)
        *initv = __initv;
}

extern "C" void
_init_libj6(uint64_t *rsp)
{
    uint64_t argc = *rsp++;
    rsp += argc;

    __initc = *rsp++;
    __initv = (struct j6_init_value *)rsp;

    for (unsigned i = 0; i < __initc; ++i) {
        if (__initv[i].type == j6_init_handle_other &&
            __initv[i].handle.type == j6_object_type_system) {
            __handle_sys = __initv[i].handle.handle;
        }
        else if (__initv[i].type == j6_init_handle_self &&
            __initv[i].handle.type == j6_object_type_process) {
            __handle_self = __initv[i].handle.handle;
        }
    }
}

#endif // __j6kernel
