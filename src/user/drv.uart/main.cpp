#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <j6/errors.h>
#include <j6/flags.h>
#include <j6/signals.h>
#include <j6/syscalls.h>
#include <j6/sysconf.h>
#include <j6/types.h>
#include <util/constexpr_hash.h>

#include "io.h"
#include "serial.h"

extern "C" {
    int main(int, const char **);
}

constexpr j6_handle_t handle_self = 1;
constexpr j6_handle_t handle_sys = 2;

struct entry
{
    uint8_t bytes;
    uint8_t area;
    uint8_t severity;
    uint8_t sequence;
    char message[0];
};

static const uint8_t level_colors[] = {0x07, 0x07, 0x0f, 0x0b, 0x09};
const char *level_names[] = {"", "debug", "info", "warn", "error", "fatal"};
const char *area_names[] = {
#define LOG(name, lvl) #name ,
#include <j6/tables/log_areas.inc>
#undef LOG
    nullptr
};

constexpr size_t in_buf_size = 512;
constexpr size_t out_buf_size = 128 * 1024;

uint8_t com1_in[in_buf_size];
uint8_t com2_in[in_buf_size];
uint8_t com1_out[out_buf_size];
uint8_t com2_out[out_buf_size];

serial_port *g_com1;
serial_port *g_com2;

constexpr size_t stack_size = 0x10000;
constexpr uintptr_t stack_top = 0xf80000000;

void
print_header()
{
    char stringbuf[150];

    unsigned version_major = j6_sysconf(j6sc_version_major);
    unsigned version_minor = j6_sysconf(j6sc_version_minor);
    unsigned version_patch = j6_sysconf(j6sc_version_patch);
    unsigned version_git = j6_sysconf(j6sc_version_gitsha);

    size_t len = snprintf(stringbuf, sizeof(stringbuf),
            "\e[38;5;21mjsix OS\e[38;5;8m %d.%d.%d (%07x) booting...\e[0m\r\n",
            version_major, version_minor, version_patch, version_git);
    g_com1->write(stringbuf, len);
}

void
log_pump_proc()
{
    size_t buffer_size = 0;
    void *message_buffer = nullptr;
    char stringbuf[300];

    j6_status_t result = j6_system_request_iopl(handle_sys, 3);
    if (result != j6_status_ok)
        return;

    while (true) {
        size_t size = buffer_size;
        j6_status_t s = j6_system_get_log(handle_sys, message_buffer, &size);

        if (s == j6_err_insufficient) {
            free(message_buffer);
            buffer_size = size * 2;
            message_buffer = malloc(buffer_size);
            continue;
        } else if (s != j6_status_ok) {
            j6_log("uart driver got error from get_log");
            continue;
        }

        if (size == 0) {
            j6_signal_t sigs = 0;
            j6_kobject_wait(handle_sys, j6_signal_system_has_log, &sigs);
            continue;
        }

        const entry *e = reinterpret_cast<entry*>(message_buffer);

        const char *area_name = area_names[e->area];
        const char *level_name = level_names[e->severity];
        uint8_t level_color = level_colors[e->severity];

        size_t len = snprintf(stringbuf, sizeof(stringbuf),
                "\e[38;5;%dm%7s %5s: %s\e[38;5;0m\r\n",
                level_color, area_name, level_name, e->message);
        g_com1->write(stringbuf, len);
    }
}

int
main(int argc, const char **argv)
{
    j6_log("uart driver starting");

    j6_handle_t endp = j6_handle_invalid;
    j6_status_t result = j6_status_ok;

    result = j6_system_request_iopl(handle_sys, 3);
    if (result != j6_status_ok)
        return result;

    result = j6_endpoint_create(&endp);
    if (result != j6_status_ok)
        return result;

    result = j6_system_bind_irq(handle_sys, endp, 3);
    if (result != j6_status_ok)
        return result;

    result = j6_system_bind_irq(handle_sys, endp, 4);
    if (result != j6_status_ok)
        return result;

    serial_port com1 {COM1, in_buf_size, com1_in, out_buf_size, com1_out};
    serial_port com2 {COM2, in_buf_size, com2_in, out_buf_size, com2_out};
    g_com1 = &com1;
    g_com2 = &com2;

    print_header();

    j6_handle_t child_stack_vma = j6_handle_invalid;
    result = j6_vma_create_map(&child_stack_vma, stack_size, stack_top-stack_size, j6_vm_flag_write);
    if (result != j6_status_ok)
        return result;

    uint64_t *sp = reinterpret_cast<uint64_t*>(stack_top - 0x10);
    sp[0] = sp[1] = 0;

    j6_handle_t child = j6_handle_invalid;
    result = j6_thread_create(&child, handle_self, stack_top - 0x10, reinterpret_cast<uintptr_t>(&log_pump_proc));
    if (result != j6_status_ok)
        return result;

    size_t len = 0;
    while (true) {
        uint64_t tag = 0;
        result = j6_endpoint_receive(endp, &tag, nullptr, &len, 10000);
        if (result == j6_err_timed_out) {
            com1.handle_interrupt();
            com2.handle_interrupt();
            continue;
        }

        if (result != j6_status_ok) {
            j6_log("uart driver got error waiting for irq");
            continue;
        }

        if (!j6_tag_is_irq(tag)) {
            j6_log("uart driver got non-irq waiting for irq");
            continue;
        }

        unsigned irq = j6_tag_to_irq(tag);
        switch (irq) {
            case 3:
                com2.handle_interrupt();
                break;
            case 4:
                com1.handle_interrupt();
                break;
            default:
                j6_log("uart driver got unknown irq waiting for irq");
        }
    }

    j6_log("uart driver somehow got to the end of main");
    return 0;
}
