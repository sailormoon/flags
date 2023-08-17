# ⛳ flags
[![Build Status](https://travis-ci.org/sailormoon/flags.svg?branch=master)](https://travis-ci.org/sailormoon/flags)

Simple, extensible, header-only C++17 argument parser released into the public domain.


<!-- vim-markdown-toc GFM -->

* [why](#why)
* [requirements](#requirements)
* [api](#api)
  * [get](#get)
  * [get (with default value)](#get-with-default-value)
  * [positional](#positional)
* [usage](#usage)
  * [example](#example)
  * [another example](#another-example)
  * [extensions](#extensions)
    * [example](#example-1)
  * [command line details](#command-line-details)
    * [key formatting](#key-formatting)
    * [value assignment](#value-assignment)
      * [bools](#bools)
* [testing](#testing)
* [contributing](#contributing)

<!-- vim-markdown-toc -->

# why
Other argument parsers are:
- bloated
- non-extensible
- not modern
- complicated

# requirements
GCC 7.0 or Clang 4.0.0 at a minimum. This library makes extensive use of `optional`, `nullopt`, and `string_view`.

# api
`flags::args` exposes seven methods:

## get
`std::optional<T> get(const std::string_view& key) const`

Attempts to parse the given key on the command-line. If the string is malformed or the argument was not passed, returns `nullopt`. Otherwise, returns the parsed type as an optional.

## get (with default value)
`T get(const std::string_view& key, T&& default_value) const`

Functions the same as `get`, except if the value is malformed or the key was not provided, returns `default_value`. Otherwise, returns the parsed type.

## get_multiple
`std::vector<std::optional<T>> get_multiple(const std::string_view& option) const`

Get all values passed for an option. If no value is specified (`--foo --bar`) or the value is malformed, `nullopt` will be used. Values will be in the order they were passed.

## get_multiple (with default value)
`std::vector<T> get_multiple(const std::string_view& option, T&& default_value) const`

Functions the same as `get_multiple`, except if the value is malformed or no value is provided, `default_value` will be used.

## get (positional)
`std::optional<T> get(size_t positional_index) const`

Get an argument from the positional arguments at a specified index. If the value is malformed or the index is invalid, `nullopt` is returned.

## get (positional with default value)
`T get(size_t positional_index, T&& default_value) const`

Functions the same as positional `get`, except if the value is malformed or the index is invalid, returns `default_value`. Otherwise, returns the parsed type.

## positional
`const std::vector<std::string_view>& positional() const`

Returns all of the positional arguments from argv in order.

# usage
### just the headers
Just include `flags.h` from the `include` directory into your project.

## Using CMake

### CMake Installation

Flags can be built and installed using [CMake], e.g.

```sh
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make install
```

The above will install Flags into the standard installation path on a UNIX
system, e.g. `/usr/local/include/`. To change the installation path, use:

```sh
$ cmake .. -DCMAKE_INSTALL_PREFIX=../install
```

in the above.

### `find_package`

Installation creates a `flags-config.cmake` which allows CMake
projects to find Flags using `find_package`:

```cmake
find_package(flags)
```

This exports the `flags` target which can be linked against any other
target. Linking against `flags` automatically sets the include
directories and required flags for C++17 or later. For example:

```cmake
add_executable(myexe mysources...)
target_link_libraries(myexe PRIVATE flags)
```

### `add_subdirectory`

The Flags can also be added as a dependency with `add_subdirectory`:

```cmake
add_subdirectory(path/to/flags)
```

This also exports the `flags` target which can be linked against any
other target just as with the installation case.

## example
```c++
#include "flags.h" // #include <flags.h> for cmake
#include <iostream>

int main(int argc, char** argv) {
  const flags::args args(argc, argv);

  const auto count = args.get<int>("count");
  if (!count) {
    std::cerr << "No count supplied. :(\n";
    return 1;
  }
  std::cout << "That's " << *count << " incredible, colossal credits!\n";

  if (args.get<bool>("laugh", false)) {
    std::cout << "Ha ha ha ha!\n";
  }
  return 0;
}
```
```bash
$ ./program
> No count supplied. :(
```
```bash
$ ./program --count=5 --laugh
> That's 5 incredible, colossal credits!
> Ha ha ha ha!
```

## another example
```c++
#include "flags.h" // #include <flags.h> for cmake
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  const flags::args args(argc, argv);
  const auto& files = args.positional();
  const auto verbose = args.get<bool>("verbose", false);
  if (verbose) {
    std::cout << "I'm a verbose program! I'll be reading the following files:\n";
    for (const auto& file : files) {
      std::cout << "* " << file << '\n';
    }
  }
  // read files(files);
  return 0;
}
```
```bash
$ ./program /tmp/one /tmp/two /tmp/three --verbose
> I'm a verbose program! I'll be reading the following files:
> * /tmp/one
> * /tmp/two
> * /tmp/three
```
```bash
$ ./program /tmp/one /tmp/two /tmp/three --noverbose
>%
```

## extensions
`flags` simply uses the `istream` operator to parse values from `argv`. To extend the parser to support your own types, just supply an overloaded `>>`.

### example
```c++
struct Date {
  int day;
  int month;
  int year;
};

// Custom parsing code.
std::istream& operator>>(std::istream& stream, Date& date) {
  return stream >> date.day >> date.month >> date.year;
}

int main(int argc, char** argv) {
  const flags::args args(argc, argv);
  if (const auto date = args.get<Date>("date")) {
    // Output %Y/%m/%d if a date was provided.
    std::cout << date->year << ":" << date->month << ":" << date->day << '\n';
    return 0;
  }
  // Sad face if no date was provided or if the input was malformed.
  std::cerr << ":(\n";
  return 1;
}
```

```bash
$ ./program --date="10 11 2016"
> 2016:11:10
```

```bash
$ ./program
> :(
```

## command line details
`flags`'s primary goal is to be simple to use for both the user and programmer.

### key formatting
A key can have any number of preceding `-`s, but must have more than 0.
The following are valid keys:
- `-key`
- `--key`
- `-------------key`

### value assignment
A value can be assigned to a key in one of two ways:
- `$ ./program --key=value`
- `$ ./program --key value`

#### bools
booleans are a special case. The following values make an argument considered `false`-y when parsed as a bool:
- `f`
- `false`
- `n`
- `no`
- `0`

If none of these conditions are met, the bool is considered `true`.

# testing
flags uses both [bfg9000](https://github.com/jimporter/bfg9000) and [mettle](https://github.com/jimporter/mettle) for unit-testing. After installing both `bfg9000` and `mettle`, run the following commands to kick off the tests:

1. `9k build/`
2. `cd build`
3. `ninja test`

# contributing
Contributions of any variety are greatly appreciated. All code is passed through `clang-format` using the Google style.
