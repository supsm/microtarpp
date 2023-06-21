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
};

constexpr size_t mtar_raw_header_size = mtar_raw_header_info::_padding_offset + mtar_raw_header_info::_padding_size;
constexpr size_t mtar_record_size = 512;
static_assert(mtar_raw_header_size == mtar_record_size);

size_t mtar_t::round_up(size_t n, size_t incr)
{
	return n + (incr - n % incr) % incr;
}

unsigned int mtar_t::checksum(const mtar_raw_header_t& rh)
{
	using namespace mtar_raw_header_info;
	unsigned i;
	unsigned res = 8 * 32;
	// skip checksum data while computing checksum
	for (size_t i = 0; i < checksum_offset; i++)
	{
		res += static_cast<unsigned char>(rh[i]);
	}
	// include "padding" data in checksum otherwise UStar archives cannot be read
	for (size_t i = checksum_offset + checksum_size; i < mtar_raw_header_size; i++)
	{
		res += static_cast<unsigned char>(rh[i]);
	}
	return res;
}

mtar_error mtar_t::tread(char* data, size_t size)
{
	mtar_error err = read_func(*this, data, size);
	read_pos += size;
	return err;
}

mtar_error mtar_t::twrite(const char* data, size_t size)
{
	mtar_error err = write_func(*this, data, size);
	write_pos += size;
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
	h.name = std::string(rh.data() + name_offset);
	h.linkname = std::string(rh.data() + linkname_offset, linkname_size);

	return mtar_error::SUCCESS;
}

mtar_error mtar_t::header_to_raw(mtar_raw_header_t& rh, const mtar_header_t& h)
{
	using namespace mtar_raw_header_info;

	rh = {};

	/* Load header into raw header */
	// there should be no errors so we don't need to handle any
	std::to_chars(rh.data() + mode_offset, rh.data() + mode_offset + mode_size, h.mode, 8);
	std::to_chars(rh.data() + owner_offset, rh.data() + owner_offset + owner_size, h.owner, 8);
	std::to_chars(rh.data() + size_offset, rh.data() + size_offset + size_size, h.size, 8);
	std::to_chars(rh.data() + mtime_offset, rh.data() + mtime_offset + mtime_size, h.mtime, 8);
	rh[type_offset] = static_cast<unsigned int>(h.type);
	if (h.name.size() >= 100)
	{
		// leave 1 for null
		std::copy_n(h.name.begin(), 99, rh.data() + name_offset);
	}
	else
	{
		std::copy(h.name.begin(), h.name.end(), rh.data() + name_offset);
	}
	if (h.linkname.size() >= 100)
	{
		std::copy_n(h.linkname.begin(), 99, rh.data() + linkname_offset);
	}
	else
	{
		std::copy(h.linkname.begin(), h.linkname.end(), rh.data() + linkname_offset);
	}

	/* Calculate and write checksum */
	unsigned int chksum = checksum(rh);
	// use aux array for leading zeros
	// c-array is more convenient here
	char aux[6] = { '0', '0', '0', '0', '0', '0' };
	auto res = std::to_chars(aux, aux + 7, chksum, 8);
	if (res.ptr == aux + 7) // all 6 elements are used, ptr is 1 past end
	{
		std::copy(aux, aux + 6, rh.data() + checksum_offset);
	}
	else
	{
		std::rotate_copy(aux, res.ptr, aux + 6, rh.data() + checksum_offset);
	}
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

mtar_t::mtar_t(std::istream& is) : stream{std::ref(is)}
{
	/* Init tar struct and functions */
	read_func = [](mtar_t& tar, char* data, size_t size)
	{
		std::get<1>(tar.stream).get().read(data, size);
		return mtar_error::SUCCESS;
	};
	seek_func = [](mtar_t& tar, size_t offset)
	{
		std::get<1>(tar.stream).get().seekg(offset, std::ios::beg);
		return mtar_error::SUCCESS;
	};
}

mtar_t::mtar_t(std::ostream& os) : stream{std::ref(os)}
{
	/* Init tar struct and functions */
	write_func = [](mtar_t& tar, const char* data, size_t size)
	{
		std::get<2>(tar.stream).get().write(data, size);
		return mtar_error::SUCCESS;
	};
	close_func = [](mtar_t& tar) noexcept
	{
		std::get<2>(tar.stream).get().flush();
	};
}

mtar_t::mtar_t(std::iostream& ios) : stream{std::ref(ios)}
{
	/* Init tar struct and functions */
	write_func = [](mtar_t& tar, const char* data, size_t size)
	{
		std::get<3>(tar.stream).get().write(data, size);
		return mtar_error::SUCCESS;
	};
	read_func = [](mtar_t& tar, char* data, size_t size)
	{
		std::get<3>(tar.stream).get().read(data, size);
		return mtar_error::SUCCESS;
	};
	seek_func = [](mtar_t& tar, size_t offset)
	{
		std::get<3>(tar.stream).get().seekg(offset, std::ios::beg);
		return mtar_error::SUCCESS;
	};
	close_func = [](mtar_t& tar) noexcept
	{
		std::get<3>(tar.stream).get().flush();
	};
}

mtar_t::~mtar_t()
{
	close_func(*this);
}

mtar_error mtar_t::seek(size_t pos)
{
	read_pos = pos;
	remaining_data = 0; // clear remaining data to prevent read_header, seek, read_data (error)
	return seek_func(*this, pos);
}

mtar_error mtar_t::seek_data(ptrdiff_t off)
{
	// if remaining_data is 0 (past end by unknown amount or not in data record at all)
	// or off is more than remaining_data (seeks past data)
	// or read_pos + off is less than zero
	// or read_pos + off is less than last_header + mtar_record_size (seeks before data)
	if (remaining_data == 0 || remaining_data < off || read_pos < -off || read_pos + off < last_header + mtar_record_size)
	{
		return mtar_error::SEEKFAIL;
	}
	read_pos += off;
	remaining_data -= off;
	return seek_func(*this, read_pos);
}

mtar_error mtar_t::rewind()
{
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
	size_t n = round_up(h.size, mtar_record_size);
	return seek(read_pos + n);
}

mtar_error mtar_t::skip_data(size_t data_size)
{
	return seek(read_pos + round_up(data_size, mtar_record_size));
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
		skip_data(header.size);
	}
	/* Return error */
	if (err == mtar_error::NULLRECORD)
	{
		err = mtar_error::NOTFOUND;
	}
	return err;
}

mtar_error mtar_t::peek_header(mtar_header_t& h)
{
	/* Save header position */
	last_header = read_pos;
	/* Read raw header */
	mtar_raw_header_t rh;
	mtar_error err = tread(rh.data(), mtar_raw_header_size);
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

mtar_error mtar_t::read_header(mtar_header_t& h)
{
	/* Save header position */
	last_header = read_pos;
	/* Read raw header */
	mtar_raw_header_t rh;
	mtar_error err = tread(rh.data(), mtar_raw_header_size);
	if (err != mtar_error::SUCCESS)
	{
		return err;
	}
	/* Load raw header into header struct and return */
	err = raw_to_header(h, rh);
	if (err != mtar_error::SUCCESS)
	{
		return err;
	}
	remaining_data = h.size;
	return mtar_error::SUCCESS;
}

mtar_error mtar_t::read_data(char* data, size_t size)
{
	/* If we have no remaining data then this is the first read and header is valid
	 * we consume the header, which sets the remaining data and puts us in the correct place */
	if (remaining_data == 0)
	{
		/* Read header */
		mtar_header_t h;
		mtar_error err = read_header(h);
		if (err != mtar_error::SUCCESS)
		{
			return err;
		}
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
	return twrite(rh.data(), mtar_raw_header_size);
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
		return write_null_bytes(round_up(write_pos, mtar_record_size) - write_pos);
	}
	return mtar_error::SUCCESS;
}

mtar_error mtar_t::finalize()
{
	/* Write two NULL records */
	return write_null_bytes(mtar_record_size * 2);
}
