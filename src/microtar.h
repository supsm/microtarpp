/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `microtar.c` for details.
 */

#ifndef MICROTAR_H
#define MICROTAR_H

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#define MTAR_VERSION "0.1.0"

using std::size_t;

enum class mtar_error
{
	SUCCESS = 0,
	FAILURE = -1,
	OPENFAIL = -2,
	READFAIL = -3,
	WRITEFAIL = -4,
	SEEKFAIL = -5,
	BADCHKSUM = -6,
	NULLRECORD = -7,
	NOTFOUND = -8
};

enum class mtar_type : unsigned int
{
	REG = '0',
	LNK = '1',
	SYM = '2',
	CHR = '3',
	BLK = '4',
	DIR = '5',
	FIFO = '6'
};

struct mtar_header_t
{
	unsigned mode = 664;
	unsigned owner = 0;
	unsigned size = 0;
	unsigned mtime = 0;
	mtar_type type = mtar_type::REG;
	std::string name;
	std::string linkname;
};

using mtar_raw_header_t = std::array<char, 512>;

class mtar_t
{
private:
	std::function<mtar_error(mtar_t&, char*, size_t)> read_func;
	std::function<mtar_error(mtar_t&, const char*, size_t)> write_func;
	std::function<mtar_error(mtar_t&, size_t)> seek_func;
	std::function<void(mtar_t&)> close_func = [](mtar_t& tar) noexcept {};

	static constexpr size_t NULL_BLOCKSIZE = 4096;
	static constexpr char null_block[NULL_BLOCKSIZE]{};

	static unsigned int round_up(unsigned int n, unsigned int incr);
	static unsigned int checksum(const mtar_raw_header_t& rh);
	mtar_error tread(char* data, size_t size);
	mtar_error twrite(const char* data, size_t size);
	mtar_error write_null_bytes(size_t n);
	static mtar_error raw_to_header(mtar_header_t& h, const mtar_raw_header_t& rh);
	static mtar_error header_to_raw(mtar_raw_header_t& rh, const mtar_header_t& h);


public:
	mtar_t(std::istream& is);
	mtar_t(std::ostream& os);
	mtar_t(std::iostream& ios);
	template<typename ReadFunc, typename WriteFunc, typename SeekFunc, typename CloseFunc>
	mtar_t(ReadFunc read_func_, WriteFunc write_func_, SeekFunc seek_func_, CloseFunc close_func_) :
		read_func(read_func_), write_func(write_func_), seek_func(seek_func_), close_func(close_func_) {}
	~mtar_t();

	std::variant<std::monostate,
		std::reference_wrapper<std::istream>,
		std::reference_wrapper<std::ostream>,
		std::reference_wrapper<std::iostream>> stream;
	size_t read_pos = 0;
	size_t write_pos = 0;
	size_t remaining_data = 0;
	size_t last_header = 0;

	static std::string_view strerror(mtar_error err);

	// seek READ, does not affect write
	mtar_error seek(size_t pos);
	mtar_error rewind();
	mtar_error next();
	mtar_error find(std::string_view name, mtar_header_t& h);
	mtar_error read_header(mtar_header_t& h);
	mtar_error read_data(char* ptr, size_t size);

	mtar_error write_header(const mtar_header_t& h);
	mtar_error write_file_header(std::string_view name, size_t size);
	mtar_error write_dir_header(std::string_view name);
	mtar_error write_data(const char* data, size_t size);
	mtar_error finalize();
};

std::string_view mtar_strerror(mtar_error err);

#endif
