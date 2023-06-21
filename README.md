# microtar++
A lightweight tar library written in C++17

## Differences
The main difference between microtar++ and the original microtar is of course
that microtar++ uses C++ streams and classes instead of C `FILE*` and struct.
However, there are also some API changes:
- `mtar_read_header` is replaced by `mtar_t::peek_header`, `mtar_t::read_header`
  does not seek back to before the header, "consuming" the header.
- `mtar_t::skip_data` is provided to skip over data only (no header). This
  was not necessary in the original microtar because there would not have been
  any scenario where only the header was consumed

## Basic Usage
The library consists of `microtar.cpp` and `microtar.h`. These two files can be
dropped into an existing project and compiled along with it.


#### Reading
```c++
// open archive for reading
std::ifstream fin("test.tar", std::ios::binary);
mtar_t tar(fin);

// print all file names and sizes
mtar_header_t h;
while (tar.read_header(h) == mtar_error::SUCCESS)
{
  std::cout << h.name << " (" << h.size << " bytes)" << '\n';
  tar.next();
}

// load and print contents of file `text.txt`
tar.find("text.txt", h);
std::vector<char> v(h.size);
tar.read_data(v.data(), h.size);
// print vector
std::copy(v.begin(), v.end(), std::ostream_iterator<char>(std::cout));
std::cout << std::endl;
```

#### Writing
```c++
// open archive for writing
std::ofstream fout("test.tar", std::ios::binary);
mtar_t tar(fout);

// write strings to files `test1.txt` and `test2.txt`
std::string str1 = "Hello World", str2 = "Goodbye world";
tar.write_file_header("test1.txt", str1.size());
tar.write_data(str1.data(), str1.size());
tar.write_file_header("test2.txt", str2.size());
tar.write_data(str2.data(), str2.size());

// finalize, needs to be the last thing done
tar.finalize();
```


## Error handling
All functions which return an `mtar_error` will return `mtar_error::SUCCESS`
if the operation is successful. If an error occurs an error value less-than-zero
will be returned; this value can be passed to the static function
`mtar_t::strerror()` to get its corresponding error string.


## Custom I/O
If you want to read or write from something other than a stream, the `mtar_t`
class can be constructed from own callback functions. `std::function` can be
default constructed (e.g. using `{}`) if certain functions such as writing
are not supported.

All callback functions are passed a reference to the `mtar_t` object as their
first argument. They should return `mtar_error::SUCCESS` if the operation
succeeds without an error, or an integer below zero if an error occurs (see
`mtar_error` enum values).

#### Reading
The following constructor arguments should be provided for reading an archive
from a stream:

Name         | Function Arguments                       | Description
-------------|------------------------------------------|---------------------------
`read_func`  | `mtar_t& tar, char* data, size_t size`   | Read data from the stream
`seek_func`  | `mtar_t& tar, size_t pos`                | Set the position indicator
`close_func` | `mtar_t& tar`                            | Close the stream

#### Writing
The following argument should be provided for writing an archive to a stream:

Name         | Function Arguments                             | Description
-------------|------------------------------------------------|---------------------
`write_func` | `mtar_t& tar, const char* data, size_t size`   | Write data to the stream


## License
This library is free software; you can redistribute it and/or modify it under
the terms of the MIT license. See [LICENSE](LICENSE) for details.
