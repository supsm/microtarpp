/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `microtar.c` for details.
 */

#ifndef MICROTAR_H
#define MICROTAR_H

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#define MTAR_VERSION "0.1.0"

using std::size_t;

enum
{
	MTAR_ESUCCESS = 0,
	MTAR_EFAILURE = -1,
	MTAR_EOPENFAIL = -2,
	MTAR_EREADFAIL = -3,
	MTAR_EWRITEFAIL = -4,
	MTAR_ESEEKFAIL = -5,
	MTAR_EBADCHKSUM = -6,
	MTAR_ENULLRECORD = -7,
	MTAR_ENOTFOUND = -8
};

enum
{
	MTAR_TREG = '0',
	MTAR_TLNK = '1',
	MTAR_TSYM = '2',
	MTAR_TCHR = '3',
	MTAR_TBLK = '4',
	MTAR_TDIR = '5',
	MTAR_TFIFO = '6'
};

struct mtar_header_t
{
	unsigned mode;
	unsigned owner;
	unsigned size;
	unsigned mtime;
	unsigned type;
	std::string name;
	std::string linkname;
};

class mtar_t
{
public:
	mtar_t(std::iostream& ios);

	std::function<int(mtar_t&, char*, size_t)> read;
	std::function<int(mtar_t&, const char*, size_t)> write;
	std::function<int(mtar_t&, size_t)> seek;
	std::iostream& stream;
	size_t pos = 0;
	size_t remaining_data = 0;
	size_t last_header = 0;
};

const char* mtar_strerror(int err);

int mtar_seek(mtar_t& tar, size_t pos);
int mtar_rewind(mtar_t& tar);
int mtar_next(mtar_t& tar);
int mtar_find(mtar_t& tar, std::string_view name, mtar_header_t& h);
int mtar_read_header(mtar_t& tar, mtar_header_t& h);
int mtar_read_data(mtar_t& tar, void* ptr, size_t size);

int mtar_write_header(mtar_t& tar, const mtar_header_t& h);
int mtar_write_file_header(mtar_t& tar, std::string_view name, size_t size);
int mtar_write_dir_header(mtar_t& tar, std::string_view name);
int mtar_write_data(mtar_t& tar, const void* data, size_t size);
int mtar_finalize(mtar_t& tar);

#endif
