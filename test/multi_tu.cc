#include "flags.h"

#include <doctest/doctest.h>

#include <array>
#include <string>

TEST_CASE("multi-TU inclusion links without ODR violations") {
  constexpr std::array<const char*, 4> argv = {"app", "--flag", "42", "pos"};
  const flags::args parsed(argv);
  CHECK(parsed.program() == "app");
  CHECK(parsed.get<int>("flag") == 42);
  CHECK(parsed.get<std::string>(0) == "pos");
}
