#include <stddef.h>
#include <uefi/types.h>

#include "console.h"
#include "error.h"
#include "memory.h"

namespace boot {
namespace memory {

using mem_entry = kernel::args::mem_entry;
using mem_type = kernel::args::mem_type;

size_t fixup_pointer_index = 0;
void **fixup_pointers[64];

static const wchar_t *memory_type_names[] = {
	L"reserved memory type",
	L"loader code",
	L"loader data",
	L"boot services code",
	L"boot services data",
	L"runtime services code",
	L"runtime services data",
	L"conventional memory",
	L"unusable memory",
	L"acpi reclaim memory",
	L"acpi memory nvs",
	L"memory mapped io",
	L"memory mapped io port space",
	L"pal code",
	L"persistent memory"
};

static const wchar_t *
memory_type_name(uefi::memory_type t)
{
	if (t < uefi::memory_type::max_memory_type) {
		return memory_type_names[static_cast<uint32_t>(t)];
	}

	switch(t) {
		case args_type:		return L"jsix kernel args";
		case module_type:	return L"jsix bootloader module";
		case kernel_type:	return L"jsix kernel code";
		case table_type:	return L"jsix page tables";
		default: return L"Bad Type Value";
	}
}

void
update_marked_addresses(uefi::event, void *context)
{
	uefi::runtime_services *rs =
		reinterpret_cast<uefi::runtime_services*>(context);

	for (size_t i = 0; i < fixup_pointer_index; ++i) {
		if (fixup_pointers[i])
			rs->convert_pointer(0, fixup_pointers[i]);
	}
}

void
init_pointer_fixup(uefi::boot_services *bs, uefi::runtime_services *rs)
{
	status_line status(L"Initializing pointer virtualization event");

	uefi::event event;
	bs->set_mem(&fixup_pointers, sizeof(fixup_pointers), 0);
	fixup_pointer_index = 0;

	try_or_raise(
		bs->create_event(
			uefi::evt::signal_virtual_address_change,
			uefi::tpl::callback,
			(uefi::event_notify)&update_marked_addresses,
			rs,
			&event),
		L"Error creating memory virtualization event");
}

void
mark_pointer_fixup(void **p)
{
	if (fixup_pointer_index == 0) {
		const size_t count = sizeof(fixup_pointers) / sizeof(void*);
		for (size_t i = 0; i < count; ++i) fixup_pointers[i] = 0;
	}
	fixup_pointers[fixup_pointer_index++] = p;
}

/*
void
memory_virtualize(EFI_RUNTIME_SERVICES *runsvc, struct memory_map *map)
{
	memory_mark_pointer_fixup((void **)&runsvc);
	memory_mark_pointer_fixup((void **)&map);

	// Get the pointer to the start of PML4
	uint64_t* cr3 = 0;
	__asm__ __volatile__ ( "mov %%cr3, %0" : "=r" (cr3) );

	// PML4 is indexed with bits 39:47 of the virtual address
	uint64_t offset = (KERNEL_VIRT_ADDRESS >> 39) & 0x1ff;

	// Double map the lower half pages that are present into the higher half
	for (unsigned i = 0; i < offset; ++i) {
		if (cr3[i] & 0x1)
			new_pml4[i] = new_pml4[offset+i] = cr3[i];
		else
			new_pml4[i] = new_pml4[offset+i] = 0;
	}

	// Write our new PML4 pointer back to CR3
	__asm__ __volatile__ ( "mov %0, %%cr3" :: "r" (new_pml4) );

	EFI_MEMORY_DESCRIPTOR *end = INCREMENT_DESC(map->entries, map->length);
	EFI_MEMORY_DESCRIPTOR *d = map->entries;
	while (d < end) {
		switch (d->Type) {
		case memtype_kernel:
		case memtype_data:
		case memtype_initrd:
		case memtype_scratch:
			d->Attribute |= EFI_MEMORY_RUNTIME;
			d->VirtualStart = d->PhysicalStart + KERNEL_VIRT_ADDRESS;

		default:
			if (d->Attribute & EFI_MEMORY_RUNTIME) {
				d->VirtualStart = d->PhysicalStart + KERNEL_VIRT_ADDRESS;
			}
		}
		d = INCREMENT_DESC(d, map->size);
	}

	runsvc->SetVirtualAddressMap(map->length, map->size, map->version, map->entries);
}
*/

bool
can_merge(mem_entry &prev, mem_type type, uefi::memory_descriptor *next)
{
	return
		prev.type == type &&
		prev.start + (page_size * prev.pages) == next->physical_start &&
		prev.attr == (next->attribute & 0xffffffff);
}

efi_mem_map
get_uefi_mappings(bool allocate, uefi::boot_services *bs)
{
	status_line(L"Getting UEFI memory map");

	efi_mem_map map;
	size_t needs_size = 0;

	uefi::status status = bs->get_memory_map(
		&needs_size, nullptr, &map.key, &map.size, &map.version);

	if (status != uefi::status::buffer_too_small)
		error::raise(status, L"Error getting memory map size");

	if (allocate) {
		map.length = needs_size + 10*map.size;

		try_or_raise(
			bs->allocate_pool(
				uefi::memory_type::loader_data, map.length,
				reinterpret_cast<void**>(&map.entries)),
			L"Allocating space for memory map");

		try_or_raise(
			bs->get_memory_map(&map.length, map.entries, &map.key, &map.size, &map.version),
			L"Getting UEFI memory map");
	}

	return map;
}

size_t
build_kernel_mem_map(kernel::args::header *args, uefi::boot_services *bs)
{
	status_line(L"Creating kernel memory map");

	efi_mem_map efi_map = get_uefi_mappings(false, bs);

	size_t map_size = efi_map.num_entries() * sizeof(mem_entry);
	kernel::args::mem_entry *kernel_map = nullptr;
	try_or_raise(
		bs->allocate_pages(
			uefi::allocate_type::any_pages,
			module_type,
			bytes_to_pages(map_size),
			reinterpret_cast<void**>(&kernel_map)),
		L"Error allocating kernel memory map module space.");

	bs->set_mem(kernel_map, map_size, 0);
	efi_map = get_uefi_mappings(true, bs);

	size_t i = 0;
	bool first = true;
	for (auto desc : efi_map) {
		/*
		console::print(L"   Range %lx (%lx) %x(%s) [%lu]\r\n",
			desc->physical_start, desc->attribute, desc->type, memory_type_name(desc->type), desc->number_of_pages);
		*/

		mem_type type;
		switch (desc->type) {
			case uefi::memory_type::reserved:
			case uefi::memory_type::unusable_memory:
			case uefi::memory_type::acpi_memory_nvs:
			case uefi::memory_type::pal_code:
				continue;

			case uefi::memory_type::loader_code:
			case uefi::memory_type::loader_data:
			case uefi::memory_type::boot_services_code:
			case uefi::memory_type::boot_services_data:
			case uefi::memory_type::conventional_memory:
				type = mem_type::free;
				break;

			case uefi::memory_type::runtime_services_code:
			case uefi::memory_type::runtime_services_data:
				type = mem_type::uefi_runtime;
				break;

			case uefi::memory_type::acpi_reclaim_memory:
				type = mem_type::acpi;
				break;

			case uefi::memory_type::memory_mapped_io:
			case uefi::memory_type::memory_mapped_io_port_space:
				type = mem_type::mmio;
				break;

			case uefi::memory_type::persistent_memory:
				type = mem_type::persistent;
				break;

			case args_type:
				type = mem_type::args;
				break;

			case module_type:
				type = mem_type::module;
				break;

			case kernel_type:
				type = mem_type::kernel;
				break;

			case table_type:
				type = mem_type::table;
				break;

			default:
				error::raise(
					uefi::status::invalid_parameter,
					L"Got an unexpected memory type from UEFI memory map");
		}

		// TODO: validate uefi's map is sorted
		if (first) {
			first = false;
			kernel_map[i].start = desc->physical_start;
			kernel_map[i].pages = desc->number_of_pages;
			kernel_map[i].type = type;
			kernel_map[i].attr = (desc->attribute & 0xffffffff);
			continue;
		}

		mem_entry &prev = kernel_map[i];
		if (can_merge(prev, type, desc)) {
			prev.pages += desc->number_of_pages;
		} else {
			mem_entry &next = kernel_map[++i];
			next.start = desc->physical_start;
			next.pages = desc->number_of_pages;
			next.type = type;
			next.attr = (desc->attribute & 0xffffffff);
		}
	}

	kernel::args::module &module = args->modules[args->num_modules++];
	module.location = reinterpret_cast<void*>(kernel_map);
	module.size = map_size;
	module.type = kernel::args::mod_type::memory_map;

	/*
	for (size_t i = 0; i<map.num_entries(); ++i) {
		mem_entry &ent = kernel_map[i];
		console::print(L"   Range %lx (%x) %d [%lu]\r\n",
			ent.start, ent.attr, ent.type, ent.pages);
	}
	*/

	return efi_map.key;
}


} // namespace boot
} // namespace memory
