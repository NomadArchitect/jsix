#include <stddef.h>
#include <stdint.h>

#include "kutil/memory.h"
#include "console.h"
#include "device_manager.h"
#include "font.h"
#include "interrupts.h"
#include "kernel_data.h"
#include "log.h"
#include "memory.h"
#include "memory_pages.h"
#include "screen.h"

extern "C" {
	void do_the_set_registers(popcorn_data *header);
	void kernel_main(popcorn_data *header);
}

/*
console
load_console(const popcorn_data *header)
{
	console cons{
		font::load(header->font),
		screen{
			header->frame_buffer,
			header->hres,
			header->vres,
			header->rmask,
			header->gmask,
			header->bmask},
		header->log,
		header->log_length};

	return cons;
}
*/

void
kernel_main(popcorn_data *header)
{
	console *cons = new (&g_console) console();
	cons->set_color(0x21, 0x00);
	cons->puts("Popcorn OS ");
	cons->set_color(0x08, 0x00);
	cons->puts(GIT_VERSION " booting...\n");

	log::init(cons);

	page_manager *pager = new (&g_page_manager) page_manager;
	pager->mark_offset_pointer(&header->frame_buffer, header->frame_buffer_length);

	memory_initialize_managers(
			header->memory_map,
			header->memory_map_length,
			header->memory_map_desc_size);

	size_t n = 5000;
	void *p = kalloc(n);
	log::info(logs::memory, "kalloc'd %d bytes at %lx", n, p);

	interrupts_init();
	interrupts_enable();

	device_manager devices(header->acpi_table);

	// int x = 1 / 0;
	// __asm__ __volatile__("int $15");

	g_console.puts("boogity!");
	do_the_set_registers(header);
}
