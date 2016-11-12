# ⛳ flags
Simple, extensible, header-only C++17 argument parser.


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
* [contributing](#contributing)

<!-- vim-markdown-toc -->

# why
Other argument parsers are:
- bloated
- non-extensible
- not modern
- complicated

# requirements
A modern compiler and standard library supporting `optional`, `nullopt`, `string_view`, and `make_array` in `std::experimental`.

# api
`flags::args` exposes three methods:

## get
`std::optional<T> get(const std::string_view& key) const`

Attempts to parse the given key on the command-line. If the string is malformed or the argument was not passed, returns `nullopt`. Otherwise, returns the parsed type as an optional.

## get (with default value)
`T get(const std::string_view& key, T&& default_value) const`

Functions the same as `get`, except if the value is malformed or the key was not provided, returns `default_value`. Otherwise, returns the parsed type.

## positional
`const std::vector<std::string_view>& positional() const`

Returns all of the positional arguments from argv in order.

# usage

## example
```c++
#include "flags.h"
#include <iostream>

int main(int argc, char** argv) {
  const flags::args args(argc, argv);

  const auto count = args.get<int>("count");
  if (!count) {
    std::cerr << "No count supplied. :(" << std::endl;
    return 1;
  }
  std::cout << "That's " << *count << " incredible, colossal credits!" << std::endl;

  if (args.get<bool>("laugh", false)) {
    std::cout << "Ha ha ha ha!" << std::endl;
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
#include "flags.h"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  const flags::args args(argc, argv);
  const auto& files = args.positional();
  const auto verbose = args.get<bool>("verbose", false);
  if (verbose) {
    std::cout << "I'm a verbose program! I'll be reading the following files: " << std::endl;
    for (const auto& file : files) {
      std::cout << "* " << file << std::endl;
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
    // Output %Y:%m:%d if a date was provided.
    std::cout << date->day << ":" << date->month << ":" << date->year << std::endl;
  } else {
    // Sad face if no date was provided or if the input was malformed.
    std::cout << ":(" << std::endl;
  }
  return 0;
}
```

```bash
$ ./program --date="2016 11 10"
> 2016:11:10
```

```bash
$ ./program"
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

# contributing
Contributions of any variety are greatly appreciated. All code is passed through `clang-format` using the Google style.
