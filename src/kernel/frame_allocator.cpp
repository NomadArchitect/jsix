#include "kutil/assert.h"
#include "kutil/memory.h"
#include "frame_allocator.h"

using memory::frame_size;
using memory::page_offset;
using frame_block_node = kutil::list_node<frame_block>;

frame_allocator g_frame_allocator;

int
frame_block::compare(const frame_block *rhs) const
{
	if (address < rhs->address)
		return -1;
	else if (address > rhs->address)
		return 1;
	return 0;
}


frame_allocator::raw_alloc::raw_alloc(frame_allocator &fa) : m_fa(fa) {}

void *
frame_allocator::raw_alloc::allocate(size_t size)
{
	kassert(size <= frame_size, "Raw allocator only allocates a single page");

	uintptr_t addr = 0;
	if (size <= frame_size)
		m_fa.allocate(1, &addr);
	return reinterpret_cast<void*>(addr + page_offset);
}

void
frame_allocator::raw_alloc::free(void *p)
{
	m_fa.free(reinterpret_cast<uintptr_t>(p), 1);
}


frame_allocator::frame_allocator() :
	m_raw_alloc(*this)
{
}

size_t
frame_allocator::allocate(size_t count, uintptr_t *address)
{
	kassert(!m_free.empty(), "frame_allocator::pop_frames ran out of free frames!");
	if (m_free.empty())
		return 0;

	auto *first = m_free.front();

	if (count >= first->count) {
		*address = first->address;
		m_free.remove(first);
		return first->count;
	} else {
		first->count -= count;
		*address = first->address + (first->count * frame_size);
		return count;
	}
}

inline uintptr_t end(frame_block *node) { return node->address + node->count * frame_size; }

void
frame_allocator::free(uintptr_t address, size_t count)
{
	frame_block_node *node =
		reinterpret_cast<frame_block_node*>(address + page_offset);

	kutil::memset(node, 0, sizeof(frame_block_node));
	node->address = address;
	node->count = count;

	m_free.sorted_insert(node);

	frame_block_node *next = node->next();
	if (next && end(node) == next->address) {
		node->count += next->count;
		m_free.remove(next);
	}

	frame_block_node *prev = node->prev();
	if (prev && end(prev) == address) {
		prev->count += node->count;
		m_free.remove(node);
	}
}
