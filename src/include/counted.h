#pragma once
/// \file counted.h
/// Definition of the `counted` template class

#include "pointer_manipulation.h"

/// A pointer and an associated count. Memory pointed to is not managed.
/// Depending on the usage, the count may be size or number of elements.
/// Helper methods provide the ability to treat the pointer like an array.
template <typename T>
struct counted
{
	T *pointer;
	size_t count;

	/// Index this object as an array of type T
	inline T & operator [] (int i) { return pointer[i]; }

	/// Index this object as a const array of type T
	inline const T & operator [] (int i) const { return pointer[i]; }

	using iterator = offset_iterator<T>;
	using const_iterator = const_offset_iterator<T>;

	/// Return an iterator to the beginning of the array
	inline iterator begin() { return iterator(pointer, sizeof(T)); }

	/// Return an iterator to the end of the array
	inline iterator end() { return offset_ptr<T>(pointer, sizeof(T)*count); }

	/// Return an iterator to the beginning of the array
	inline const_iterator begin() const { return const_iterator(pointer, sizeof(T)); }

	/// Return an iterator to the end of the array
	inline const_iterator end() const { return offset_ptr<const T>(pointer, sizeof(T)*count); }
};

/// Specialize for `void` which cannot be indexed or iterated
template <>
struct counted<void>
{
	void *pointer;
	size_t count;
};

using buffer = counted<void>;
