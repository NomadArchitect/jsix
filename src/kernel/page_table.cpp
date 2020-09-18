#include "kutil/assert.h"
#include "kutil/memory.h"
#include "console.h"
#include "frame_allocator.h"
#include "kernel_memory.h"
#include "page_table.h"

using memory::page_offset;
using level = page_table::level;

extern frame_allocator &g_frame_allocator;

free_page_header * page_table::s_page_cache = nullptr;
size_t page_table::s_cache_count = 0;

// Flags: 0 0 0 0  0 0 0 0  0 0 1 1 = 0x0003
//        IGNORED  | | | |  | | | +- Present
//                 | | | |  | | +--- Writeable
//                 | | | |  | +----- Usermode access (Supervisor only)
//                 | | | |  +------- PWT (determining memory type for pdpt)
//                 | | | +---------- PCD (determining memory type for pdpt)
//                 | | +------------ Accessed flag (not accessed yet)
//                 | +-------------- Ignored
//                 +---------------- Reserved 0 (Table pointer, not page)
/// Page table entry flags for entries pointing at another table
constexpr uint16_t table_flags = 0x003;


page_table::iterator::iterator(uintptr_t virt, page_table *pml4) :
	m_table {pml4, 0, 0, 0}
{
	for (unsigned i = 0; i < D; ++i)
		m_index[i] = static_cast<uint16_t>((virt >> (12 + 9*(3-i))) & 0x1ff);
}

page_table::iterator::iterator(const page_table::iterator &o)
{
	kutil::memcpy(&m_table, &o.m_table, sizeof(m_table));
	kutil::memcpy(&m_index, &o.m_index, sizeof(m_index));
}

inline static level to_lv(unsigned i) { return static_cast<level>(i); }
inline static unsigned to_un(level i) { return static_cast<unsigned>(i); }

uintptr_t
page_table::iterator::start(level l) const
{
	uintptr_t address = 0;

	for (level i = level::pml4; i <= l; ++i)
		address |= static_cast<uintptr_t>(index(i)) << (12 + 9*(3-unsigned(i)));

	// canonicalize the address
	if (address & (1ull<<47))
		address |= (0xffffull<<48);

	return address;
}

uintptr_t
page_table::iterator::end(level l) const
{
	kassert(l != level::pml4, "Called end() with level::pml4");

	uintptr_t address = 0;

	for (level i = level::pml4; i < l; ++i) {
		uintptr_t idx = index(i) +
			((i == l) ? 1 : 0);
		address |= idx << (12 + 9*(3-unsigned(i)));
	}

	// canonicalize the address
	if (address & (1ull<<47))
		address |= (0xffffull<<48);

	return address;
}

level
page_table::iterator::align() const
{
	for (int i = 4; i > 0; --i)
		if (m_index[i-1]) return level(i);
	return level::pml4;
}

page_table::level
page_table::iterator::depth() const
{
	for (level i = level::pml4; i < level::pt; ++i)
		if (!(entry(i) & 1)) return i;
	return level::pt;
}

void
page_table::iterator::next(level l)
{
	kassert(l != level::pml4, "Called next() with level::pml4");
	kassert(l <= level::page, "Called next() with too deep level");

	for (level i = l; i < level::page; ++i)
		index(i) = 0;

	while (++index(--l) == 512) {
		kassert(to_un(l), "iterator ran off the end of memory");
		index(l) = 0;
	}
}

bool
page_table::iterator::allowed() const
{
	level d = depth();
	while (true) {
		if (entry(d) & flag_allowed) return true;
		else if (d == level::pml4) return false;
		--d;
	}
}

void
page_table::iterator::allow(level at, bool allowed)
{
	for (level l = level::pdp; l <= at; ++l)
		ensure_table(l);

	if (allowed) entry(at) |= flag_allowed;
	else entry(at) &= ~flag_allowed;
}

bool
page_table::iterator::operator!=(const iterator &o) const
{
	for (unsigned i = 0; i < D; ++i)
		if (o.m_index[i] != m_index[i])
			return true;

	return o.m_table[0] != m_table[0];
}

bool
page_table::iterator::check_table(level l) const
{
	// We're only dealing with D levels of paging, and
	// there must always be a PML4.
	if (l == level::pml4 || l > level::pt)
		return l == level::pml4;

	uint64_t parent = entry(l - 1);
	if (parent & 1) {
		table(l) = reinterpret_cast<page_table*>(page_offset | (parent & ~0xfffull));
		return true;
	}
	return false;
}

void
page_table::iterator::ensure_table(level l)
{
	// We're only dealing with D levels of paging, and
	// there must always be a PML4.
	if (l == level::pml4 || l > level::pt) return;
	if (check_table(l)) return;

	uintptr_t phys = 0;
	size_t n = g_frame_allocator.allocate(1, &phys);
	kassert(n, "Failed to allocate a page table");

	uint64_t &parent = entry(l - 1);
	uint64_t flags = table_flags |
		(parent & flag_allowed) ? flag_allowed : 0;

	m_table[unsigned(l)] = reinterpret_cast<page_table*>(phys | page_offset);
	parent  = (reinterpret_cast<uintptr_t>(phys) & ~0xfffull) | flags;
}

page_table *
page_table::get(int i, uint16_t *flags) const
{
	uint64_t entry = entries[i];

	if ((entry & 0x1) == 0)
		return nullptr;

	if (flags)
		*flags = entry & 0xfffull;

	return reinterpret_cast<page_table *>((entry & ~0xfffull) + page_offset);
}

void
page_table::set(int i, page_table *p, uint16_t flags)
{
	if (entries[i] & flag_allowed) flags |= flag_allowed;
	entries[i] =
		(reinterpret_cast<uint64_t>(p) - page_offset) |
		(flags & 0xfff);
}

struct free_page_header { free_page_header *next; };

page_table *
page_table::get_table_page()
{
	if (!s_cache_count)
		fill_table_page_cache();

	free_page_header *page = s_page_cache;
	s_page_cache = s_page_cache->next;
	--s_cache_count;

	return reinterpret_cast<page_table*>(page);
}

void
page_table::free_table_page(page_table *pt)
{
	free_page_header *page =
		reinterpret_cast<free_page_header*>(pt);
	page->next = s_page_cache;
	s_page_cache = page->next;
	++s_cache_count;
}

void
page_table::fill_table_page_cache()
{
	constexpr size_t min_pages = 16;

	while (s_cache_count < min_pages) {
		uintptr_t phys = 0;
		size_t n = g_frame_allocator.allocate(min_pages - s_cache_count, &phys);

		free_page_header *start =
			memory::to_virtual<free_page_header>(phys);

		for (int i = 0; i < n - 1; ++i)
			kutil::offset_pointer(start, i * memory::frame_size)
				->next = kutil::offset_pointer(start, (i+1) * memory::frame_size);

		free_page_header *end =
			kutil::offset_pointer(start, (n-1) * memory::frame_size);

		end->next = s_page_cache;
		s_page_cache = start;
		s_cache_count += n;
	}
}

void
page_table::dump(page_table::level lvl, bool recurse)
{
	console *cons = console::get();

	cons->printf("\nLevel %d page table @ %lx:\n", lvl, this);
	for (int i=0; i<memory::table_entries; ++i) {
		uint64_t ent = entries[i];

		if ((ent & 0x1) == 0)
			cons->printf("  %3d: %016lx   NOT PRESENT\n", i, ent);

		else if ((lvl == level::pdp || lvl == level::pd) && (ent & 0x80) == 0x80)
			cons->printf("  %3d: %016lx -> Large page at    %016lx\n", i, ent, ent & ~0xfffull);

		else if (lvl == level::pt)
			cons->printf("  %3d: %016lx -> Page at          %016lx\n", i, ent, ent & ~0xfffull);

		else
			cons->printf("  %3d: %016lx -> Level %d table at %016lx\n",
					i, ent, deeper(lvl), (ent & ~0xfffull) + page_offset);
	}

	if (lvl != level::pt && recurse) {
		for (int i=0; i<=memory::table_entries; ++i) {
			if (is_large_page(lvl, i))
				continue;

			page_table *next = get(i);
			if (next)
				next->dump(deeper(lvl), true);
		}
	}
}

page_table_indices::page_table_indices(uint64_t v) :
	index{
		(v >> 39) & 0x1ff,
		(v >> 30) & 0x1ff,
		(v >> 21) & 0x1ff,
		(v >> 12) & 0x1ff }
{}

uintptr_t
page_table_indices::addr() const
{
	return
		(index[0] << 39) |
		(index[1] << 30) |
		(index[2] << 21) |
		(index[3] << 12);
}

bool operator==(const page_table_indices &l, const page_table_indices &r)
{
	return l[0] == r[0] && l[1] == r[1] && l[2] == r[2] && l[3] == r[3];
}
