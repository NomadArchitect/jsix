#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "interrupts.h"

enum class gdt_flags : uint8_t
{
	ac = 0x01,
	rw = 0x02,
	dc = 0x04,
	ex = 0x08,
	r1 = 0x20,
	r2 = 0x40,
	r3 = 0x60,
	pr = 0x80,

	res_1 = 0x10
};

inline gdt_flags
operator | (gdt_flags lhs, gdt_flags rhs)
{
	return static_cast<gdt_flags>(
			static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

struct gdt_descriptor
{
   uint16_t limit_low;
   uint16_t base_low;
   uint8_t base_mid;
   uint8_t flags;
   uint8_t granularity;
   uint8_t base_high;
} __attribute__ ((packed));

struct idt_descriptor
{
   uint16_t base_low;
   uint16_t selector;
   uint8_t  ist;
   uint8_t  flags;
   uint16_t base_mid;
   uint32_t base_high;
   uint32_t reserved;		// must be zero
} __attribute__ ((packed));

struct table_ptr
{
   uint16_t limit;
   uint64_t base;
} __attribute__ ((packed));

gdt_descriptor g_gdt_table[10];
idt_descriptor g_idt_table[256];
table_ptr g_gdtr;
table_ptr g_idtr;


struct registers;

extern "C" {
	void idt_write();
	void idt_load();

	void gdt_write();
	void gdt_load();

	void isr_handler(registers);
	void irq_handler(registers);

#define ISR(i, name)     extern void name ()
#define EISR(i, name)    extern void name ()
#define IRQ(i, q, name)  extern void name ()
#include "interrupt_isrs.inc"
#undef IRQ
#undef EISR
#undef ISR
}

void idt_dump(const table_ptr &table);
void gdt_dump(const table_ptr &table);

void
set_gdt_entry(uint8_t i, uint32_t base, uint32_t limit, bool is64, gdt_flags flags)
{
   g_gdt_table[i].limit_low = limit & 0xffff;

   g_gdt_table[i].base_low = base & 0xffff;
   g_gdt_table[i].base_mid = (base >> 16) & 0xff;
   g_gdt_table[i].base_high = (base >> 24) & 0xff;

   g_gdt_table[i].granularity =
		   ((limit >> 16) & 0xf) | (is64 ? 0xa0 : 0xc0);

   g_gdt_table[i].flags = static_cast<uint8_t>(flags | gdt_flags::res_1 | gdt_flags::pr);
}

void
set_idt_entry(uint8_t i, uint64_t addr, uint16_t selector, uint8_t flags)
{
	g_idt_table[i].base_low = addr & 0xffff;
	g_idt_table[i].base_mid = (addr >> 16) & 0xffff;
	g_idt_table[i].base_high = (addr >> 32) & 0xffffffff;
	g_idt_table[i].selector = selector;
	g_idt_table[i].flags = flags;
	g_idt_table[i].ist = 0;
	g_idt_table[i].reserved = 0;
}

void
interrupts_init()
{
	uint8_t *p;

	p = reinterpret_cast<uint8_t *>(&g_gdt_table);
	for (int i = 0; i < sizeof(g_gdt_table); ++i) p[i] = 0;

	g_gdtr.limit = sizeof(g_gdt_table) - 1;
	g_gdtr.base = reinterpret_cast<uint64_t>(&g_gdt_table);

	set_gdt_entry(1, 0, 0xfffff, false, gdt_flags::rw);
	set_gdt_entry(2, 0, 0xfffff, false, gdt_flags::rw | gdt_flags::ex | gdt_flags::dc);
	set_gdt_entry(3, 0, 0xfffff, false, gdt_flags::rw);
	set_gdt_entry(4, 0, 0xfffff, false, gdt_flags::rw | gdt_flags::ex);

	set_gdt_entry(6, 0, 0xfffff, false, gdt_flags::rw);
	set_gdt_entry(7, 0, 0xfffff,  true, gdt_flags::rw | gdt_flags::ex);

	gdt_write();
	p = reinterpret_cast<uint8_t *>(&g_idt_table);
	for (int i = 0; i < sizeof(g_idt_table); ++i) p[i] = 0;

	g_idtr.limit = sizeof(g_idt_table) - 1;
	g_idtr.base = reinterpret_cast<uint64_t>(&g_idt_table);

#define ISR(i, name)     set_idt_entry(i, reinterpret_cast<uint64_t>(& name), 0x38, 0x8e);
#define EISR(i, name)    set_idt_entry(i, reinterpret_cast<uint64_t>(& name), 0x38, 0x8e);
#define IRQ(i, q, name)  set_idt_entry(i, reinterpret_cast<uint64_t>(& name), 0x38, 0x8e);
#include "interrupt_isrs.inc"
#undef IRQ
#undef EISR
#undef ISR

	idt_write();
}

struct registers
{
	uint64_t ds;
	uint64_t rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
	uint64_t interrupt, errorcode;
	uint64_t rip, cs, eflags, user_esp, ss;
};

#define print_reg(name, value) \
	cons->puts("         " name ": "); \
	cons->put_hex((value)); \
	cons->puts("\n");

void
isr_handler(registers regs)
{
	console *cons = console::get();

	cons->puts("received ISR interrupt:\n");

	print_reg("ISR", regs.interrupt);
	print_reg("ERR", regs.errorcode);
	console::get()->puts("\n");

	print_reg(" ds", regs.ds);
	print_reg("rdi", regs.rdi);
	print_reg("rsi", regs.rsi);
	print_reg("rbp", regs.rbp);
	print_reg("rsp", regs.rsp);
	print_reg("rbx", regs.rbx);
	print_reg("rdx", regs.rdx);
	print_reg("rcx", regs.rcx);
	print_reg("rax", regs.rax);
	console::get()->puts("\n");

	print_reg("rip", regs.rip);
	print_reg(" cs", regs.cs);
	print_reg(" ef", regs.eflags);
	print_reg("esp", regs.user_esp);
	print_reg(" ss", regs.ss);
	while(1) asm("hlt");
}

void
irq_handler(registers regs)
{
	console *cons = console::get();

	cons->puts("received IRQ interrupt:\n");

	print_reg("ISR", regs.interrupt);
	print_reg("ERR", regs.errorcode);
	console::get()->puts("\n");

	print_reg(" ds", regs.ds);
	print_reg("rdi", regs.rdi);
	print_reg("rsi", regs.rsi);
	print_reg("rbp", regs.rbp);
	print_reg("rsp", regs.rsp);
	print_reg("rbx", regs.rbx);
	print_reg("rdx", regs.rdx);
	print_reg("rcx", regs.rcx);
	print_reg("rax", regs.rax);
	console::get()->puts("\n");

	print_reg("rip", regs.rip);
	print_reg(" cs", regs.cs);
	print_reg(" ef", regs.eflags);
	print_reg("esp", regs.user_esp);
	print_reg(" ss", regs.ss);
	while(1) asm("hlt");
}


void
gdt_dump(const table_ptr &table)
{
	console *cons = console::get();

	cons->puts("Loaded GDT at: ");
	cons->put_hex(table.base);
	cons->puts(" size: ");
	cons->put_dec(table.limit + 1);
	cons->puts(" bytes\n");

	int count = (table.limit + 1) / sizeof(gdt_descriptor);
	const gdt_descriptor *gdt =
		reinterpret_cast<const gdt_descriptor *>(table.base);
	for (int i = 0; i < count; ++i) {
		uint32_t base =
			(gdt[i].base_high << 24) |
			(gdt[i].base_mid << 16) |
			gdt[i].base_low;

		uint32_t limit =
			static_cast<uint32_t>(gdt[i].granularity & 0x0f) << 16 |
			gdt[i].limit_low;

		cons->puts("   Entry ");
		cons->put_dec(i);

		cons->puts(": Base ");
		cons->put_hex(base);

		cons->puts("  Limit ");
		cons->put_hex(limit);

		cons->puts("  Privs ");
		cons->put_dec((gdt[i].flags >> 5) & 0x03);

		cons->puts("  Flags ");

		if (gdt[i].flags & 0x80) {
			cons->puts("P ");

			if (gdt[i].flags & 0x08)
				cons->puts("EX ");
			else
				cons->puts("   ");

			if (gdt[i].flags & 0x04)
				cons->puts("DC ");
			else
				cons->puts("   ");

			if (gdt[i].flags & 0x02)
				cons->puts("RW ");
			else
				cons->puts("   ");

			if (gdt[i].granularity & 0x80)
				cons->puts("KB ");
			else
				cons->puts(" B ");

			if ((gdt[i].granularity & 0x60) == 0x20)
				cons->puts("64");
			else if ((gdt[i].granularity & 0x60) == 0x40)
				cons->puts("32");
			else
				cons->puts("16");
		}

		cons->puts("\n");
	}
}

void
idt_dump(const table_ptr &table)
{
	console *cons = console::get();

	cons->puts("Loaded IDT at: ");
	cons->put_hex(table.base);
	cons->puts(" size: ");
	cons->put_dec(table.limit + 1);
	cons->puts(" bytes\n");

	int count = (table.limit + 1) / sizeof(idt_descriptor);
	const idt_descriptor *idt =
		reinterpret_cast<const idt_descriptor *>(table.base);

	if (count > 32) count = 32;
	for (int i = 0; i < count; ++i) {
		uint64_t base =
			(static_cast<uint64_t>(idt[i].base_high) << 32) |
			(static_cast<uint64_t>(idt[i].base_mid)  << 16) |
			idt[i].base_low;

		cons->puts("   Entry ");
		cons->put_dec(i);

		cons->puts(": Base ");
		cons->put_hex(base);

		cons->puts("  Sel(");
		cons->put_dec(idt[i].selector & 0x3);
		cons->puts(",");
		cons->put_dec((idt[i].selector & 0x4) >> 2);
		cons->puts(",");
		cons->put_dec(idt[i].selector >> 3);
		cons->puts(") ");

		cons->puts("IST ");
		cons->put_dec(idt[i].ist);

		switch (idt[i].flags & 0xf) {
			case 0x5: cons->puts(" 32tsk "); break;
			case 0x6: cons->puts(" 16int "); break;
			case 0x7: cons->puts(" 16trp "); break;
			case 0xe: cons->puts(" 32int "); break;
			case 0xf: cons->puts(" 32trp "); break;
			default:
				cons->puts("   ?");
				cons->put_hex(idt[i].flags & 0xf);
				cons->puts(" ");
				break;
		}

		cons->puts("DPL ");
		cons->put_dec((idt[i].flags >> 5) & 0x3);

		if (idt[i].flags & 0x80)
			cons->puts(" P");

		cons->puts("\n");
	}
}