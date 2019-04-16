#pragma once
/// \file slab_allocator.h
/// A slab allocator and related definitions
#include "kutil/allocator.h"
#include "kutil/assert.h"
#include "kutil/linked_list.h"
#include "kutil/memory.h"

namespace kutil {


/// A slab allocator for small structures kept in a linked list
template <typename T, size_t N = memory::frame_size>
class slab_allocator :
	public linked_list<T>
{
public:
	using item_type = list_node<T>;

	/// Default constructor.
	/// \arg chunk_size The size of chunk to allocate, in bytes. 0 means default.
	/// \arg alloc      The allocator to use to allocate chunks. Defaults to malloc().
	slab_allocator(allocator &alloc) :
		m_alloc(alloc)
	{
	}

	/// Get an item from the cache. May allocate a new chunk if the cache is empty.
	/// \returns  An allocated element
	inline item_type * pop()
	{
		if (this->empty()) allocate();
		kassert(!this->empty(), "Slab allocator is empty after allocate()");
		item_type *item = this->pop_front();
		kutil::memset(item, 0, sizeof(item_type));
		return item;
	}

	/// Return an item to the cache.
	/// \arg item  A previously allocated element
	inline void push(item_type *item)
	{
		this->push_front(item);
	}

	void allocate()
	{
		constexpr unsigned count = N / sizeof(item_type);

		void *memory = m_alloc.allocate(N);
		item_type *items = reinterpret_cast<item_type *>(memory); 
		for (size_t i = 0; i < count; ++i)
			this->push_back(&items[i]);
	}

private:
	allocator& m_alloc;
};

} // namespace kutil
