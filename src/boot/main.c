#include <efi.h>
#include <efilib.h>
#include <stdalign.h>
#include <stddef.h>

#include "console.h"
#include "guids.h"
#include "loader.h"
#include "memory.h"
#include "utility.h"

#ifndef GIT_VERSION
#define GIT_VERSION L"no version"
#endif

#define KERNEL_HEADER_MAGIC   0x600db007
#define KERNEL_HEADER_VERSION 1

#define DATA_HEADER_MAGIC     0x600dda7a
#define DATA_HEADER_VERSION   1


#pragma pack(push, 1)
struct kernel_header {
	uint32_t magic;
	uint16_t version;
	uint16_t length;

	uint8_t major;
	uint8_t minor;
	uint16_t patch;
	uint32_t gitsha;

	void *entrypoint;
};

struct popcorn_data {
	uint32_t magic;
	uint16_t version;
	uint16_t length;

	uint32_t _reserved0;
	uint32_t flags;

	void *font;
	size_t font_length;

	void *data;
	size_t data_length;

	EFI_MEMORY_DESCRIPTOR *memory_map;
	EFI_RUNTIME_SERVICES *runtime;

	void *acpi_table;

	void *frame_buffer;
	size_t frame_buffer_size;
	uint32_t hres;
	uint32_t vres;
	uint32_t rmask;
	uint32_t gmask;
	uint32_t bmask;
	uint32_t _reserved1;
}
__attribute__((aligned(_Alignof(EFI_MEMORY_DESCRIPTOR))));
#pragma pack(pop)

EFI_STATUS
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
	EFI_STATUS status;
	EFI_BOOT_SERVICES *bootsvc = system_table->BootServices;

	// When checking console initialization, use CHECK_EFI_STATUS_OR_RETURN
	// because we can't be sure if the console was fully set up
	status = con_initialize(system_table, GIT_VERSION);
	CHECK_EFI_STATUS_OR_RETURN(status, "con_initialize");
	// From here on out, we can use CHECK_EFI_STATUS_OR_FAIL instead

	// Find ACPI tables. Ignore ACPI 1.0 if a 2.0 table is found.
	//
	void *acpi_table = NULL;
	for (size_t i=0; i<system_table->NumberOfTableEntries; ++i) {
		EFI_CONFIGURATION_TABLE *efi_table = &system_table->ConfigurationTable[i];
		if (is_guid(&efi_table->VendorGuid, &guid_acpi2)) {
			acpi_table = efi_table->VendorTable;
			break;
		} else if (is_guid(&efi_table->VendorGuid, &guid_acpi1)) {
			// Mark a v1 table with the LSB high
			acpi_table = (void *)((intptr_t)efi_table->VendorTable | 0x1);
		}
	}

	// Compute necessary number of data pages
	//
	size_t data_length = 0;
	status = memory_get_map_length(bootsvc, &data_length);
	CHECK_EFI_STATUS_OR_FAIL(status);

	size_t header_size = sizeof(struct popcorn_data);
	const size_t header_align = alignof(struct popcorn_data);
	if (header_size % header_align)
		header_size += header_align - (header_size % header_align);

	data_length += header_size;

	// Load the kernel image from disk and check it
	//
	void *kernel_image = NULL, *kernel_data = NULL;
	uint64_t kernel_length = 0;
	con_printf(L"Loading kernel into memory...\r\n");

	struct loader_data load;
	load.kernel_data_length = data_length;
	status = loader_load_kernel(bootsvc, &load);
	CHECK_EFI_STATUS_OR_FAIL(status);

	con_printf(L"    %u image bytes at 0x%x\r\n", load.kernel_image_length, load.kernel_image);
	con_printf(L"    %u font bytes at 0x%x\r\n", load.screen_font_length, load.screen_font);
	con_printf(L"    %u data bytes at 0x%x\r\n", load.kernel_data_length, load.kernel_data);

	struct kernel_header *version = (struct kernel_header *)load.kernel_image;
	if (version->magic != KERNEL_HEADER_MAGIC) {
		con_printf(L"    bad magic %x\r\n", version->magic);
		CHECK_EFI_STATUS_OR_FAIL(EFI_CRC_ERROR);
	}

	con_printf(L"    Kernel version %d.%d.%d %x%s\r\n", version->major, version->minor, version->patch,
		  version->gitsha & 0x0fffffff, version->gitsha & 0xf0000000 ? "*" : "");
	con_printf(L"    Entrypoint 0x%x\r\n", version->entrypoint);

	void (*kernel_main)() = version->entrypoint;

	// Set up the kernel data pages to pass to the kernel
	//
	struct popcorn_data *data_header = (struct popcorn_data *)load.kernel_data; 

	data_header->magic = DATA_HEADER_MAGIC;
	data_header->version = DATA_HEADER_VERSION;
	data_header->length = sizeof(struct popcorn_data);

	data_header->flags = 0;

	data_header->font = load.screen_font;
	data_header->font_length = load.screen_font_length;

	data_header->data = load.screen_font;
	data_header->data_length = load.screen_font_length;

	data_header->memory_map = (EFI_MEMORY_DESCRIPTOR *)(data_header + 1);
	data_header->runtime = system_table->RuntimeServices;
	data_header->acpi_table = acpi_table;

	data_header->_reserved0 = 0;
	data_header->_reserved1 = 0;

	// Figure out the framebuffer (if any) and add that to the data header
	//
	status = con_get_framebuffer(
			bootsvc, 
			&data_header->frame_buffer,
			&data_header->frame_buffer_size,
			&data_header->hres,
			&data_header->vres,
			&data_header->rmask,
			&data_header->gmask,
			&data_header->bmask);
	CHECK_EFI_STATUS_OR_FAIL(status);

	// Save the memory map and tell the firmware we're taking control.
	//
	struct memory_map map;
	map.entries = data_header->memory_map;
	map.length = (load.kernel_data_length - header_size);

	status = memory_get_map(bootsvc, &map);
	CHECK_EFI_STATUS_OR_FAIL(status);

	status = bootsvc->ExitBootServices(image_handle, map.key);
	CHECK_EFI_STATUS_OR_ASSERT(status, 0);

	// Hand control to the kernel
	//
	kernel_main(data_header);
	return EFI_LOAD_ERROR;
}