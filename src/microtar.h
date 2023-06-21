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
	REG = '0', // normal file
	LNK = '1', // hard link
	SYM = '2', // symlink
	CHR = '3', // character device
	BLK = '4', // block device
	DIR = '5', // directory
	FIFO = '6' // named pipe
};

struct mtar_header_t
{
	unsigned mode = 0664; // posix mode (read/write/execute)
	unsigned owner = 0; // owner of file
	unsigned size = 0; // size of file (in bytes)
	unsigned mtime = 0; // unix timestamp when file was last modified
	mtar_type type = mtar_type::REG; // type of file
	std::string name; // filename
	std::string linkname; // name of link destination
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

	static size_t round_up(size_t n, size_t incr);
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
	mtar_t(std::function<mtar_error(mtar_t&, char*, size_t)> read_func_,
		std::function<mtar_error(mtar_t&, const char*, size_t)> write_func_,
		std::function<mtar_error(mtar_t&, size_t)> seek_func_,
		std::function<void(mtar_t&)> close_func_) :
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

	// get error message
	static std::string_view strerror(mtar_error err);

	// seek READ, does not affect write
	mtar_error seek(size_t pos);
	// rewind reading to beginning of file
	mtar_error rewind();
	// skip to next record (file + data)
	mtar_error next();
	// skip over data section, if header is already read
	mtar_error skip_data(size_t data_size);
	// find entry in archive
	mtar_error find(std::string_view name, mtar_header_t& h);
	// read header and seek back to original position
	mtar_error peek_header(mtar_header_t& h);
	// read and consume header
	mtar_error read_header(mtar_header_t& h);
	// read and consume data
	mtar_error read_data(char* ptr, size_t size);

	// write custom header data
	mtar_error write_header(const mtar_header_t& h);
	// write header data for file entry
	mtar_error write_file_header(std::string_view name, size_t size);
	// write header data for directory entry
	mtar_error write_dir_header(std::string_view name);
	// write file data (not header)
	mtar_error write_data(const char* data, size_t size);
	// mark end of archive
	mtar_error finalize();
};

#endif
