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
#include <array>
#include <charconv>

#include "microtar.h"

using mtar_raw_header_t = std::array<char, 512>;

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

	static_assert(_padding_offset + _padding_size == 512);
};

static unsigned round_up(unsigned n, unsigned incr)
{
	return n + (incr - n % incr) % incr;
}

static unsigned checksum(const mtar_raw_header_t& rh)
{
	unsigned i;
	unsigned res = 8 * 32;
	for (unsigned char c : rh)
	{
		res += c;
	}
	return res;
}

static int tread(mtar_t& tar, char* data, size_t size)
{
	int err = tar.read(tar, data, size);
	tar.pos += size;
	return err;
}

static int twrite(mtar_t& tar, const char* data, size_t size)
{
	int err = tar.write(tar, data, size);
	tar.pos += size;
	return err;
}

static int write_null_bytes(mtar_t& tar, int n)
{
	int err;
	std::vector<char> v(n, '\0');
	err = twrite(tar, v.data(), n);
	if (err)
	{
		return err;
	}
	return MTAR_ESUCCESS;
}

static int raw_to_header(mtar_header_t& h, const mtar_raw_header_t& rh)
{
	using namespace mtar_raw_header_info;
	unsigned chksum1, chksum2;

	/* If the checksum starts with a null byte we assume the record is NULL */
	if (rh[checksum_offset] == '\0')
	{
		return MTAR_ENULLRECORD;
	}

	/* Build and compare checksum */
	chksum1 = checksum(rh);
	std::errc ec = std::from_chars(
		rh.data() + checksum_offset, rh.data() + checksum_offset + checksum_size,
		chksum2, 8).ec;
	if (ec != std::errc() || chksum1 != chksum2)
	{
		return MTAR_EBADCHKSUM;
	}

	/* Load raw header into header */
	ec = std::from_chars(
		rh.data() + mode_offset, rh.data() + mode_offset + mode_size,
		h.mode, 8).ec;
	if (ec != std::errc())
	{
		return MTAR_EFAILURE;
	}
	ec = std::from_chars(
		rh.data() + owner_offset, rh.data() + owner_offset + owner_size,
		h.owner, 8).ec;
	if (ec != std::errc())
	{
		return MTAR_EFAILURE;
	}
	ec = std::from_chars(
		rh.data() + size_offset, rh.data() + size_offset + size_size,
		h.size, 8).ec;
	if (ec != std::errc())
	{
		return MTAR_EFAILURE;
	}
	ec = std::from_chars(
		rh.data() + mtime_offset, rh.data() + mtime_offset + mtime_size,
		h.mtime, 8).ec;
	if (ec != std::errc())
	{
		return MTAR_EFAILURE;
	}
	h.type = rh[type_offset];
	h.name = std::string(rh.data() + name_offset, name_size);
	h.linkname = std::string(rh.data() + linkname_offset, linkname_size);

	return MTAR_ESUCCESS;
}

static int header_to_raw(mtar_raw_header_t& rh, const mtar_header_t& h)
{
	using namespace mtar_raw_header_info;
	unsigned chksum;

	/* Load header into raw header */
	// we need snprintf for leading zeros
	// should be fine because there should be no errors
	snprintf(rh.data() + mode_offset, mode_size, "%08o", h.mode);
	snprintf(rh.data() + owner_offset, owner_size, "%08o", h.owner);
	snprintf(rh.data() + size_offset, size_size, "%08o", h.size);
	snprintf(rh.data() + mtime_offset, mtime_size, "%08o", h.mtime);
	rh[type_offset] = (h.type == 0) ? MTAR_TREG : h.type;
	// use strncpy instead of std::copy_n because size of h.name may be less than name_size
	strncpy(rh.data() + name_offset, h.name.c_str(), name_size);
	strncpy(rh.data() + linkname_offset, h.linkname.c_str(), linkname_size);

	/* Calculate and write checksum */
	chksum = checksum(rh);
	// 6 digits + null character = 7
	snprintf(rh.data() + checksum_offset, 7, "%06o", chksum);
	rh[checksum_offset + 7] = ' ';

	return MTAR_ESUCCESS;
}

const char* mtar_strerror(int err)
{
	switch (err)
	{
	case MTAR_ESUCCESS:
		return "success";
	case MTAR_EFAILURE:
		return "failure";
	case MTAR_EOPENFAIL:
		return "could not open";
	case MTAR_EREADFAIL:
		return "could not read";
	case MTAR_EWRITEFAIL:
		return "could not write";
	case MTAR_ESEEKFAIL:
		return "could not seek";
	case MTAR_EBADCHKSUM:
		return "bad checksum";
	case MTAR_ENULLRECORD:
		return "null record";
	case MTAR_ENOTFOUND:
		return "file not found";
	}
	return "unknown error";
}

static int default_write(mtar_t& tar, const char* data, size_t size)
{
	tar.stream.write(data, size);
	return MTAR_ESUCCESS;
}

static int default_read(mtar_t& tar, char* data, size_t size)
{
	tar.stream.read(data, size);
	return MTAR_ESUCCESS;
}

static int default_seek(mtar_t& tar, size_t offset)
{
	tar.stream.seekg(offset, std::ios::beg);
	return MTAR_ESUCCESS;
}

mtar_t::mtar_t(std::iostream& ios) : stream{ios}
{
	int err;
	mtar_header_t h;

	/* Init tar struct and functions */
	write = default_write;
	read = default_read;
	seek = default_seek;
}

int mtar_seek(mtar_t& tar, size_t pos)
{
	int err = tar.seek(tar, pos);
	tar.pos = pos;
	return err;
}

int mtar_rewind(mtar_t& tar)
{
	tar.remaining_data = 0;
	tar.last_header = 0;
	return mtar_seek(tar, 0);
}

int mtar_next(mtar_t& tar)
{
	int err, n;
	mtar_header_t h;
	/* Load header */
	err = mtar_read_header(tar, h);
	if (err)
	{
		return err;
	}
	/* Seek to next record */
	n = round_up(h.size, 512) + sizeof(mtar_raw_header_t);
	return mtar_seek(tar, tar.pos + n);
}

int mtar_find(mtar_t& tar, const char* name, mtar_header_t& h)
{
	int err;
	mtar_header_t header;
	/* Start at beginning */
	err = mtar_rewind(tar);
	if (err)
	{
		return err;
	}
	/* Iterate all files until we hit an error or find the file */
	while ((err = mtar_read_header(tar, header)) == MTAR_ESUCCESS)
	{
		if (header.name == name)
		{
			h = header;
			return MTAR_ESUCCESS;
		}
		mtar_next(tar);
	}
	/* Return error */
	if (err == MTAR_ENULLRECORD)
	{
		err = MTAR_ENOTFOUND;
	}
	return err;
}

int mtar_read_header(mtar_t& tar, mtar_header_t& h)
{
	int err;
	mtar_raw_header_t rh;
	/* Save header position */
	tar.last_header = tar.pos;
	/* Read raw header */
	err = tread(tar, rh.data(), sizeof(rh));
	if (err)
	{
		return err;
	}
	/* Seek back to start of header */
	err = mtar_seek(tar, tar.last_header);
	if (err)
	{
		return err;
	}
	/* Load raw header into header struct and return */
	return raw_to_header(h, rh);
}

int mtar_read_data(mtar_t& tar, char* data, size_t size)
{
	int err;
	/* If we have no remaining data then this is the first read, we get the size,
	 * set the remaining data and seek to the beginning of the data */
	if (tar.remaining_data == 0)
	{
		mtar_header_t h;
		/* Read header */
		err = mtar_read_header(tar, h);
		if (err)
		{
			return err;
		}
		/* Seek past header and init remaining data */
		err = mtar_seek(tar, tar.pos + sizeof(mtar_raw_header_t));
		if (err)
		{
			return err;
		}
		tar.remaining_data = h.size;
	}
	/* Read data */
	err = tread(tar, data, size);
	if (err)
	{
		return err;
	}
	tar.remaining_data -= size;
	/* If there is no remaining data we've finished reading and seek back to the
	 * header */
	if (tar.remaining_data == 0)
	{
		return mtar_seek(tar, tar.last_header);
	}
	return MTAR_ESUCCESS;
}

int mtar_write_header(mtar_t& tar, const mtar_header_t& h)
{
	mtar_raw_header_t rh;
	/* Build raw header and write */
	header_to_raw(rh, h);
	tar.remaining_data = h.size;
	return twrite(tar, rh.data(), sizeof(rh));
}

int mtar_write_file_header(mtar_t& tar, const char* name, size_t size)
{
	mtar_header_t h;
	/* Build header */
	h.name = name;
	h.size = size;
	h.type = MTAR_TREG;
	h.mode = 0664;
	/* Write header */
	return mtar_write_header(tar, h);
}

int mtar_write_dir_header(mtar_t& tar, const char* name)
{
	mtar_header_t h;
	/* Build header */
	h.name = name;
	h.type = MTAR_TDIR;
	h.mode = 0775;
	/* Write header */
	return mtar_write_header(tar, h);
}

int mtar_write_data(mtar_t& tar, const char* data, size_t size)
{
	int err;
	/* Write data */
	err = twrite(tar, data, size);
	if (err)
	{
		return err;
	}
	tar.remaining_data -= size;
	/* Write padding if we've written all the data for this file */
	if (tar.remaining_data == 0)
	{
		return write_null_bytes(tar, round_up(tar.pos, 512) - tar.pos);
	}
	return MTAR_ESUCCESS;
}

int mtar_finalize(mtar_t& tar)
{
	/* Write two NULL records */
	return write_null_bytes(tar, sizeof(mtar_raw_header_t) * 2);
}
