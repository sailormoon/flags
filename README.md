# ⛳ flags
[![CI](https://github.com/sailormoon/flags/actions/workflows/ci.yml/badge.svg)](https://github.com/sailormoon/flags/actions/workflows/ci.yml)

Simple, extensible, header-only C++23 argument parser released into the public domain.


<!-- vim-markdown-toc GFM -->

* [why](#why)
* [requirements](#requirements)
* [api](#api)
  * [program](#program)
  * [contains](#contains)
  * [get](#get)
  * [get (with aliases)](#get-with-aliases)
  * [get (with default value)](#get-with-default-value)
  * [get_multiple](#get_multiple)
  * [get_multiple (with aliases)](#get_multiple-with-aliases)
  * [get_multiple (with default value)](#get_multiple-with-default-value)
  * [get (positional)](#get-positional)
  * [get (positional with default value)](#get-positional-with-default-value)
  * [keys](#keys)
  * [positional](#positional)
  * [skipped](#skipped)
* [usage](#usage)
  * [just the headers](#just-the-headers)
  * [Using CMake](#using-cmake)
    * [CMake Installation](#cmake-installation)
    * [`find_package`](#find_package)
    * [`FetchContent`](#fetchcontent)
    * [`add_subdirectory`](#add_subdirectory)
  * [example](#example)
  * [another example](#another-example)
  * [extensions](#extensions)
    * [example](#example-1)
  * [command line details](#command-line-details)
    * [key formatting](#key-formatting)
    * [value assignment](#value-assignment)
      * [bools](#bools)
    * [end of options (`--`)](#end-of-options---)
    * [lifetime and parsing caveats](#lifetime-and-parsing-caveats)
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
A C++23 compiler (GCC 12+, Clang 14+, AppleClang 15+, or MSVC 2022+). This library uses `std::optional` monadic operations, `std::string_view`, `std::span`, `std::from_chars`, concepts, and ranges.

# api
`flags::args` can be constructed from `(int argc, const char* const* argv)` (compatible with `main`'s `char** argv`) or `std::span<const char* const>` and exposes the following methods:

## program
`std::string_view program() const noexcept`

Returns the program name (`argv[0]`), or an empty view if `argc <= 0`.

## contains
`bool contains(std::string_view option) const`  
`bool contains(Keys... keys) const`

Returns `true` if the given option (or any of the provided alias keys) was supplied on the command line, regardless of whether it had a value.

## get
`std::optional<T> get(std::string_view key) const`

Attempts to parse the last-supplied value for the given key on the command line. If the string is malformed or the argument was not passed, returns `std::nullopt`. Otherwise, returns the parsed type as an optional.

## get (with aliases)
`std::optional<T> get(Keys... keys) const`

Attempts to parse each key in order, returning the first match. Useful for supporting both long and short flag names:
```c++
const auto verbose = args.get<bool>("verbose", "v").value_or(false);
const auto count = args.get<int>("count", "c").value_or(0);
```

## get (with default value)
> **Deprecated:** Prefer `get<T>("key").value_or(default_value)` or the alias form `get<T>("key", "k").value_or(default_value)`.

`T get(std::string_view key, U&& default_value) const` *(disabled when `U` is convertible to `std::string_view`, which resolves to the alias overload)*

Functions the same as `get`, except if the value is malformed or the key was not provided, returns `default_value`.

## get_multiple
`std::vector<std::optional<T>> get_multiple(std::string_view option) const`

Get all values passed for an option in command-line order. If no value is specified (`--foo --bar`) or the value is malformed, `std::nullopt` is used (except for `bool`, where a valueless flag yields `true`).

## get_multiple (with aliases)
`std::vector<std::optional<T>> get_multiple(Keys... keys) const`

Collects all values across any of the given alias keys in their original command-line order (e.g. `-I /a --include /b` queried with `get_multiple<std::string>("include", "I")` returns `{"/a", "/b"}`).

## get_multiple (with default value)
> **Deprecated:** Prefer using `get_multiple<T>("key")` and handling `std::nullopt` values directly.

`std::vector<T> get_multiple(std::string_view option, U&& default_value) const` *(disabled when `U` is convertible to `std::string_view`)*

Functions the same as `get_multiple`, except if a value is malformed or missing, `default_value` is substituted.

## get (positional)
`std::optional<T> get(std::size_t positional_index) const`

Get an argument from the positional arguments at a specified index. If the value is malformed or the index is invalid, `std::nullopt` is returned.

## get (positional with default value)
> **Deprecated:** Prefer `get<T>(positional_index).value_or(default_value)`.

`T get(std::size_t positional_index, U&& default_value) const`

Functions the same as positional `get`, except if the value is malformed or the index is invalid, returns `default_value`.

## keys
`const std::vector<std::string_view>& keys() const noexcept`

Returns all unique parsed option names in order of first appearance (useful for validating unknown flags).

## positional
`const std::vector<std::string_view>& positional() const noexcept`

Returns all of the positional arguments from `argv` in order.

## skipped
`const std::vector<std::string_view>& skipped() const noexcept`

Returns all tokens appearing after the `--` end-of-options delimiter verbatim.

# usage
## just the headers
Just include `flags.h` from the `include` directory in your project.

## Using CMake

### CMake Installation

Flags can be installed using CMake:

```sh
$ cmake -S . -B build -DFLAGS_BUILD_TESTS=OFF
$ cmake --install build
```

The above installs `flags.h` and the CMake package config files into the system prefix (e.g. `/usr/local/include/flags.h`). To change the installation path, pass `--prefix`:

```sh
$ cmake --install build --prefix ./install
```

### `find_package`

Installation generates `flagsConfig.cmake`, which allows CMake projects to find Flags using `find_package`:

```cmake
find_package(flags REQUIRED)
```

This exports the `flags::flags` target, which automatically sets the include directories and C++23 compile feature:

```cmake
add_executable(myexe mysources...)
target_link_libraries(myexe PRIVATE flags::flags)
```

### `FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(
  flags
  GIT_REPOSITORY https://github.com/sailormoon/flags.git
  GIT_TAG v2.0.0)
FetchContent_MakeAvailable(flags)

add_executable(myexe mysources...)
target_link_libraries(myexe PRIVATE flags::flags)
```

### `add_subdirectory`

Flags can also be added directly with `add_subdirectory`:

```cmake
add_subdirectory(path/to/flags)
target_link_libraries(myexe PRIVATE flags::flags)
```

## example
```c++
#include "flags.h"  // #include <flags.h> for CMake
#include <iostream>

int main(int argc, char** argv) {
  const flags::args args(argc, argv);

  const auto count = args.get<int>("count");
  if (!count) {
    std::cerr << "No count supplied. :(\n";
    return 1;
  }
  std::cout << "That's " << *count << " incredible, colossal credits!\n";

  if (args.get<bool>("laugh").value_or(false)) {
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
#include "flags.h"  // #include <flags.h> for CMake
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  const flags::args args(argc, argv);
  const auto& files = args.positional();
  const auto verbose = args.get<bool>("verbose", "v").value_or(false);
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
$ ./program /tmp/one /tmp/two /tmp/three -v
> I'm a verbose program! I'll be reading the following files:
> * /tmp/one
> * /tmp/two
> * /tmp/three
```
```bash
$ ./program /tmp/one /tmp/two /tmp/three --verbose=false
```

## extensions
Built-in string, boolean, `char`, and numeric types are parsed directly (using `std::from_chars` where available for fast, strict, locale-independent numeric parsing). To extend the parser to support your own types, supply an overloaded `operator>>` for a default-initializable type.

### example
```c++
struct Date {
  int day = 0;
  int month = 0;
  int year = 0;
};

// Custom parsing code.
std::istream& operator>>(std::istream& stream, Date& date) {
  return stream >> date.day >> date.month >> date.year;
}

int main(int argc, char** argv) {
  const flags::args args(argc, argv);
  if (const auto date = args.get<Date>("date")) {
    // Output YYYY:MM:DD if a date was provided.
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
A key can have any number of preceding `-`s, but must have at least one non-dash character:
- `-key`
- `--key`
- `-------------key`

Standalone `-` (commonly used for stdin/stdout) and dash-only tokens (other than `--`) are treated as values/positional arguments rather than option keys.

### value assignment
A value can be assigned to a key in one of two ways:
- `$ ./program --key=value`
- `$ ./program --key value`

Negative numbers (such as `--offset -5` or `--delta -3.14`) are accepted as values for the preceding option as well as with `=` (`--offset=-5`).

If an option is passed multiple times (e.g. `--port 80 --port 8080`), `get` returns the **last** value (`8080`), while `get_multiple` returns all values in order (`{80, 8080}`).

#### bools
Booleans are a special case. The following values (case-insensitive) are considered `false` when parsed as a `bool`:
- `f`
- `false`
- `n`
- `no`
- `0`

If a boolean option is present without a value (e.g. `--verbose`), or has any other value, it evaluates to `true`.

### end of options (`--`)
A standalone `--` token stops flag parsing. Any pending flag before `--` is flushed as valueless, and all tokens after `--` are stored verbatim in `args.skipped()`.

### lifetime and parsing caveats
- **Non-owning views:** `flags::args` stores `std::string_view` slices into the `argv` buffers. The backing `argv` strings must outlive the `flags::args` instance.
- **Schema-less parsing:** Because `flags` does not require upfront flag declarations, a bare flag followed by a non-dash token (e.g. `./program --verbose file.txt`) assigns `file.txt` as the value of `--verbose` rather than a positional argument. Place valueless boolean flags after positional arguments, pass an explicit value (`--verbose=true file.txt`), or separate with `--`.

# testing
`flags` uses [doctest](https://github.com/doctest/doctest) via CMake `FetchContent` and CTest:

```sh
$ cmake -S . -B build
$ cmake --build build
$ ctest --test-dir build --output-on-failure
```

# contributing
Contributions of any variety are greatly appreciated. Code is formatted with `clang-format` using the repository's `.clang-format` (Google style).
