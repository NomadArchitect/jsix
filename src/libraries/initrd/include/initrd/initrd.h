#pragma once
/// \file initrd.h
/// Definitions defining the simple inital ramdisk file format used by the
/// popcorn kernel.

#include <stdint.h>
#include "kutil/vector.h"

// File format:
// 1x disk_header
// Nx file_header
// filename string data
// file data

namespace initrd {

struct disk_header;
struct file_header;


/// Encasulates methods for a file on the ramdisk
class file
{
public:
	file(const file_header *header, const void *start);

	/// Get the filename
	const char * name() const;

	/// Get the file size
	const size_t size() const;

	/// Get a pointer to the file data
	const void * data() const;

	/// Whether this file is an executable
	bool executable() const;

private:
	const file_header *m_header;
	void const *m_data;
	char const *m_name;
};


/// Encasulates access methods for the ramdisk
class disk
{
public:
	/// Constructor.
	/// \arg start  The start of the initrd in memory
	disk(const void *start, kutil::allocator &alloc);

	/// Get the vector of files on the disk
	const kutil::vector<file> & files() const { return m_files; }

private:
	kutil::vector<file> m_files;
};

} // namespace initrd
