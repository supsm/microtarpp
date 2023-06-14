/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <algorithm>
#include <charconv>

#include "microtar.h"

namespace mtar_raw_header_info
{
	constexpr size_t name_offset = 0;
	constexpr size_t name_size = 100;
	constexpr size_t mode_offset = name_offset + name_size;
	constexpr size_t mode_size = 8;
	constexpr size_t owner_offset = mode_offset + mode_size;
	constexpr size_t owner_size = 8;
	constexpr size_t group_offset = owner_offset + owner_size;
	constexpr size_t group_size = 8;
	constexpr size_t size_offset = group_offset + group_size;
	constexpr size_t size_size = 12;
	constexpr size_t mtime_offset = size_offset + size_size;
	constexpr size_t mtime_size = 12;
	constexpr size_t checksum_offset = mtime_offset + mtime_size;
	constexpr size_t checksum_size = 8;
	constexpr size_t type_offset = checksum_offset + checksum_size;
	constexpr size_t type_size = 1;
	constexpr size_t linkname_offset = type_offset + type_size;
	constexpr size_t linkname_size = 100;
	constexpr size_t _padding_offset = linkname_offset + linkname_size;
	constexpr size_t _padding_size = 255;

	constexpr size_t header_size = _padding_offset + _padding_size;
	static_assert(header_size == 512);
};

unsigned int mtar_t::round_up(unsigned int n, unsigned int incr)
{
	return n + (incr - n % incr) % incr;
}

unsigned int mtar_t::checksum(const mtar_raw_header_t& rh)
{
	unsigned i;
	unsigned res = 8 * 32;
	for (unsigned char c : rh)
	{
		res += c;
	}
	return res;
}

mtar_error mtar_t::tread(char* data, size_t size)
{
	mtar_error err = read_func(*this, data, size);
	pos += size;
	return err;
}

mtar_error mtar_t::twrite(const char* data, size_t size)
{
	mtar_error err = write_func(*this, data, size);
	pos += size;
	return err;
}

mtar_error mtar_t::write_null_bytes(size_t n)
{
	while (n > NULL_BLOCKSIZE)
	{
		mtar_error err = twrite(null_block, NULL_BLOCKSIZE);
		if (err != mtar_error::SUCCESS)
		{
			return err;
		}
		n -= NULL_BLOCKSIZE;
	}
	mtar_error err = twrite(null_block, n);
	return err;
}

mtar_error mtar_t::raw_to_header(mtar_header_t& h, const mtar_raw_header_t& rh)
{
	using namespace mtar_raw_header_info;

	/* If the checksum starts with a null byte we assume the record is NULL */
	if (rh[checksum_offset] == '\0')
	{
		return mtar_error::NULLRECORD;
	}

	/* Build and compare checksum */
	unsigned int chksum1 = checksum(rh);
	unsigned int chksum2;
	std::errc ec = std::from_chars(
		rh.data() + checksum_offset, rh.data() + checksum_offset + checksum_size,
		chksum2, 8).ec;
	if (ec != std::errc() || chksum1 != chksum2)
	{
		return mtar_error::BADCHKSUM;
	}

	/* Load raw header into header */
	ec = std::from_chars(
		rh.data() + mode_offset, rh.data() + mode_offset + mode_size,
		h.mode, 8).ec;
	if (ec != std::errc())
	{
		return mtar_error::FAILURE;
	}
	ec = std::from_chars(
		rh.data() + owner_offset, rh.data() + owner_offset + owner_size,
		h.owner, 8).ec;
	if (ec != std::errc())
	{
		return mtar_error::FAILURE;
	}
	ec = std::from_chars(
		rh.data() + size_offset, rh.data() + size_offset + size_size,
		h.size, 8).ec;
	if (ec != std::errc())
	{
		return mtar_error::FAILURE;
	}
	ec = std::from_chars(
		rh.data() + mtime_offset, rh.data() + mtime_offset + mtime_size,
		h.mtime, 8).ec;
	if (ec != std::errc())
	{
		return mtar_error::FAILURE;
	}
	h.type = static_cast<mtar_type>(rh[type_offset]);
	h.name = std::string(rh.data() + name_offset, name_size);
	h.linkname = std::string(rh.data() + linkname_offset, linkname_size);

	return mtar_error::SUCCESS;
}

mtar_error mtar_t::header_to_raw(mtar_raw_header_t& rh, const mtar_header_t& h)
{
	using namespace mtar_raw_header_info;

	/* Load header into raw header */
	// we need snprintf for leading zeros
	// should be fine because there should be no errors
	snprintf(rh.data() + mode_offset, mode_size, "%08o", h.mode);
	snprintf(rh.data() + owner_offset, owner_size, "%08o", h.owner);
	snprintf(rh.data() + size_offset, size_size, "%08o", h.size);
	snprintf(rh.data() + mtime_offset, mtime_size, "%08o", h.mtime);
	rh[type_offset] = static_cast<unsigned int>(h.type);
	// use strncpy instead of std::copy_n because size of h.name may be less than name_size
	strncpy(rh.data() + name_offset, h.name.c_str(), name_size);
	strncpy(rh.data() + linkname_offset, h.linkname.c_str(), linkname_size);

	/* Calculate and write checksum */
	unsigned int chksum = checksum(rh);
	// 6 digits + null character = 7
	snprintf(rh.data() + checksum_offset, 7, "%06o", chksum);
	rh[checksum_offset + 7] = ' ';

	return mtar_error::SUCCESS;
}

std::string_view mtar_t::strerror(mtar_error err)
{
	switch (err)
	{
	case mtar_error::SUCCESS:
		return "success";
	case mtar_error::FAILURE:
		return "failure";
	case mtar_error::OPENFAIL:
		return "could not open";
	case mtar_error::READFAIL:
		return "could not read";
	case mtar_error::WRITEFAIL:
		return "could not write";
	case mtar_error::SEEKFAIL:
		return "could not seek";
	case mtar_error::BADCHKSUM:
		return "bad checksum";
	case mtar_error::NULLRECORD:
		return "null record";
	case mtar_error::NOTFOUND:
		return "file not found";
	}
	return "unknown error";
}

mtar_t::mtar_t(std::iostream& ios) : stream{std::ref(ios)}
{
	/* Init tar struct and functions */
	write_func = [](mtar_t& tar, const char* data, size_t size)
	{
		tar.stream.value().get().write(data, size);
		return mtar_error::SUCCESS;
	};
	read_func = [](mtar_t& tar, char* data, size_t size)
	{
		tar.stream.value().get().read(data, size);
		return mtar_error::SUCCESS;
	};
	seek_func = [](mtar_t& tar, size_t offset)
	{
		tar.stream.value().get().seekg(offset, std::ios::beg);
		return mtar_error::SUCCESS;
	};
	// TODO: do we flush here? what if stream is read-only?
	close_func = [](mtar_t& tar)noexcept
	{
		tar.stream.value().get().flush();
	};
}

mtar_t::~mtar_t()
{
	close_func(*this);
}

mtar_error mtar_t::seek(size_t pos_)
{
	pos = pos_;
	return seek_func(*this, pos_);
}

mtar_error mtar_t::rewind()
{
	remaining_data = 0;
	last_header = 0;
	return seek_func(*this, 0);
}

mtar_error mtar_t::next()
{
	/* Load header */
	mtar_header_t h;
	mtar_error err = read_header(h);
	if (err != mtar_error::SUCCESS)
	{
		return err;
	}
	/* Seek to next record */
	int n = round_up(h.size, 512) + mtar_raw_header_info::header_size;
	return seek(pos + n);
}

mtar_error mtar_t::find(std::string_view name, mtar_header_t& h)
{
	/* Start at beginning */
	mtar_error err = rewind();
	if (err != mtar_error::SUCCESS)
	{
		return err;
	}
	/* Iterate all files until we hit an error or find the file */
	mtar_header_t header;
	while ((err = read_header(header)) == mtar_error::SUCCESS)
	{
		if (header.name == name)
		{
			h = header;
			return mtar_error::SUCCESS;
		}
		next();
	}
	/* Return error */
	if (err == mtar_error::NULLRECORD)
	{
		err = mtar_error::NOTFOUND;
	}
	return err;
}

mtar_error mtar_t::read_header(mtar_header_t& h)
{
	/* Save header position */
	last_header = pos;
	/* Read raw header */
	mtar_raw_header_t rh;
	mtar_error err = tread(rh.data(), mtar_raw_header_info::header_size);
	if (err != mtar_error::SUCCESS)
	{
		return err;
	}
	/* Seek back to start of header */
	err = seek(last_header);
	if (err != mtar_error::SUCCESS)
	{
		return err;
	}
	/* Load raw header into header struct and return */
	return raw_to_header(h, rh);
}

mtar_error mtar_t::read_data(char* data, size_t size)
{
	/* If we have no remaining data then this is the first read, we get the size,
	 * set the remaining data and seek to the beginning of the data */
	if (remaining_data == 0)
	{
		/* Read header */
		mtar_header_t h;
		mtar_error err = read_header(h);
		if (err != mtar_error::SUCCESS)
		{
			return err;
		}
		/* Seek past header and init remaining data */
		err = seek(pos + mtar_raw_header_info::header_size);
		if (err != mtar_error::SUCCESS)
		{
			return err;
		}
		remaining_data = h.size;
	}
	/* Read data */
	mtar_error err = tread(data, size);
	if (err != mtar_error::SUCCESS)
	{
		return err;
	}
	remaining_data -= size;
	/* If there is no remaining data we've finished reading and seek back to the
	 * header */
	if (remaining_data == 0)
	{
		return seek(last_header);
	}
	return mtar_error::SUCCESS;
}

mtar_error mtar_t::write_header(const mtar_header_t& h)
{
	/* Build raw header and write */
	mtar_raw_header_t rh;
	header_to_raw(rh, h);
	remaining_data = h.size;
	return twrite(rh.data(), mtar_raw_header_info::header_size);
}

mtar_error mtar_t::write_file_header(std::string_view name, size_t size)
{
	/* Build header */
	mtar_header_t h;
	h.name = name;
	h.size = size;
	h.type = mtar_type::REG;
	h.mode = 0664;
	/* Write header */
	return write_header(h);
}

mtar_error mtar_t::write_dir_header(std::string_view name)
{
	/* Build header */
	mtar_header_t h;
	h.name = name;
	h.type = mtar_type::DIR;
	h.mode = 0775;
	/* Write header */
	return write_header(h);
}

mtar_error mtar_t::write_data(const char* data, size_t size)
{
	/* Write data */
	mtar_error err = twrite(data, size);
	if (err != mtar_error::SUCCESS)
	{
		return err;
	}
	remaining_data -= size;
	/* Write padding if we've written all the data for this file */
	if (remaining_data == 0)
	{
		return write_null_bytes(round_up(pos, 512) - pos);
	}
	return mtar_error::SUCCESS;
}

mtar_error mtar_t::finalize()
{
	/* Write two NULL records */
	return write_null_bytes(mtar_raw_header_info::header_size * 2);
}
