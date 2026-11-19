#include "flags.h"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace doctest {
template <typename T>
struct StringMaker<std::optional<T>> {
  static String convert(const std::optional<T>& value) {
    if (!value) return "std::nullopt";
    return toString(*value);
  }
};

template <>
struct StringMaker<std::nullopt_t> {
  static String convert(const std::nullopt_t&) { return "std::nullopt"; }
};
}  // namespace doctest

namespace {

// RAII fixture that owns argv string storage and builds a flags::args instance.
struct args_fixture {
  explicit args_fixture(std::initializer_list<const char*> items,
                        std::string program = "TEST") {
    storage_.reserve(items.size() + 1);
    storage_.push_back(std::move(program));
    for (const char* item : items) {
      storage_.emplace_back(item);
    }
    init();
  }

  explicit args_fixture(const std::vector<std::string>& items,
                        std::string program = "TEST") {
    storage_.reserve(items.size() + 1);
    storage_.push_back(std::move(program));
    for (const auto& item : items) {
      storage_.push_back(item);
    }
    init();
  }

  args_fixture(const args_fixture&) = delete;
  args_fixture& operator=(const args_fixture&) = delete;

  [[nodiscard]] const flags::args& args() const { return *args_; }

 private:
  void init() {
    argv_.reserve(storage_.size() + 1);
    for (auto& s : storage_) {
      argv_.push_back(s.data());
    }
    argv_.push_back(nullptr);
    args_.emplace(static_cast<int>(storage_.size()), argv_.data());
  }

  std::vector<std::string> storage_;
  std::vector<char*> argv_;
  std::optional<flags::args> args_;
};

struct Date {
  int day = 0;
  int month = 0;
  int year = 0;

  bool operator==(const Date&) const = default;
};

std::istream& operator>>(std::istream& stream, Date& date) {
  return stream >> date.day >> date.month >> date.year;
}

std::ostream& operator<<(std::ostream& stream, const Date& date) {
  return stream << date.year << '-' << date.month << '-' << date.day;
}

}  // namespace

TEST_SUITE("initialization and lifetime") {
  TEST_CASE("empty and null argv inputs are handled safely") {
    const flags::args zero_args(0, nullptr);
    CHECK(zero_args.program().empty());
    CHECK(zero_args.keys().empty());
    CHECK(zero_args.positional().empty());
    CHECK(zero_args.skipped().empty());

    const flags::args negative_argc(-5, nullptr);
    CHECK(negative_argc.program().empty());
    CHECK(negative_argc.positional().empty());

    const flags::args null_with_positive_argc(3, nullptr);
    CHECK(null_with_positive_argc.program().empty());

    const std::array<const char*, 3> with_null = {"prog", "--foo", nullptr};
    const flags::args parsed_with_null(with_null);
    CHECK(parsed_with_null.program() == "prog");
    CHECK(parsed_with_null.get<bool>("foo") == true);
  }

  TEST_CASE("program name and span constructor") {
    const std::array<const char*, 3> argv = {"./my_tool", "--count", "7"};
    const flags::args parsed(argv);
    CHECK(parsed.program() == "./my_tool");
    CHECK(parsed.get<int>("count") == 7);
  }

  TEST_CASE("args is copyable and movable") {
    const std::array<const char*, 4> argv = {"prog", "--x", "10", "pos"};
    flags::args original(argv);
    flags::args copied = original;
    CHECK(copied.get<int>("x") == 10);
    CHECK(copied.get<std::string_view>(0) == "pos");

    flags::args moved = std::move(copied);
    CHECK(moved.get<int>("x") == 10);
    CHECK(moved.program() == "prog");
  }
}

TEST_SUITE("positional arguments") {
  TEST_CASE("absence") {
    const args_fixture fixture({"--no", "positional", "--arguments"});
    CHECK(fixture.args().positional().empty());
    CHECK(fixture.args().get<int>(0) == std::nullopt);
    CHECK(fixture.args().get<int>(0).value_or(3) == 3);
  }

  TEST_CASE("basic") {
    const args_fixture fixture({"positional", "arguments"});
    REQUIRE(fixture.args().positional().size() == 2);
    CHECK(fixture.args().positional()[0] == "positional");
    CHECK(fixture.args().positional()[1] == "arguments");

    CHECK(fixture.args().get<std::string_view>(0) == "positional");
    CHECK(fixture.args().get<std::string_view>(1) == "arguments");
    CHECK(fixture.args().get<std::string_view>(2) == std::nullopt);
    CHECK(fixture.args().get<std::string>(0).value_or("default") ==
          "positional");
    CHECK(fixture.args().get<std::string>(1).value_or("default") ==
          "arguments");
    CHECK(fixture.args().get<std::string>(2).value_or("default") == "default");
  }

  TEST_CASE("with options") {
    const args_fixture fixture(
        {"positional", "arguments", "-with", "--some", "---options"});
    REQUIRE(fixture.args().positional().size() == 2);
    CHECK(fixture.args().positional()[0] == "positional");
    CHECK(fixture.args().positional()[1] == "arguments");
    CHECK(fixture.args().get<std::string_view>(0) == "positional");
    CHECK(fixture.args().get<std::string_view>(1) == "arguments");
    CHECK(fixture.args().get<std::string_view>(2) == std::nullopt);
  }

  TEST_CASE("with flags and options") {
    const args_fixture fixture(
        {"--flag", "\"not positional\"", "positional", "--another-flag", "foo",
         "arguments", "--bar", "42", "--some", "--options", "--foobaz"});
    REQUIRE(fixture.args().positional().size() == 2);
    CHECK(fixture.args().positional()[0] == "positional");
    CHECK(fixture.args().positional()[1] == "arguments");
  }

  TEST_CASE("typed positional parsing including bool and numbers") {
    const args_fixture fixture(
        {"false", "FALSE", "no", "0", "true", "yes", "42", "3.5", "bad", ","});
    CHECK(fixture.args().get<bool>(0) == false);
    CHECK(fixture.args().get<bool>(1) == false);
    CHECK(fixture.args().get<bool>(2) == false);
    CHECK(fixture.args().get<bool>(3) == false);
    CHECK(fixture.args().get<bool>(4) == true);
    CHECK(fixture.args().get<bool>(5) == true);
    CHECK(fixture.args().get<int>(6) == 42);
    CHECK(fixture.args().get<double>(7) == doctest::Approx(3.5));
    CHECK(fixture.args().get<int>(8) == std::nullopt);
    CHECK(fixture.args().get<char>(9) == ',');
    CHECK(fixture.args().get<char>(0) == std::nullopt);
  }
}

TEST_SUITE("flag parsing") {
  TEST_CASE("bool") {
    const args_fixture fixture({"--foo", "1", "--bar", "no", "--verbose"});
    CHECK(fixture.args().get<bool>("foo") == true);
    CHECK(fixture.args().get<bool>("foo").value_or(false) == true);
    CHECK(fixture.args().get<bool>("bar") == false);
    CHECK(fixture.args().get<bool>("verbose") == true);
    CHECK(fixture.args().get<bool>("nonexistent") == std::nullopt);
  }

  TEST_CASE("falsities are case-insensitive") {
    for (const auto falsity : flags::detail::falsities) {
      std::string lower(falsity);
      const args_fixture fixture_lower({"--foo", lower.c_str()});
      CHECK(fixture_lower.args().get<bool>("foo") == false);

      std::string upper = lower;
      for (char& c : upper) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
      }
      const args_fixture fixture_upper({"--foo", upper.c_str()});
      CHECK(fixture_upper.args().get<bool>("foo") == false);
    }

    const args_fixture mixed({"--a=False", "--b=No", "--c=True", "--d=Yes"});
    CHECK(mixed.args().get<bool>("a") == false);
    CHECK(mixed.args().get<bool>("b") == false);
    CHECK(mixed.args().get<bool>("c") == true);
    CHECK(mixed.args().get<bool>("d") == true);
  }

  TEST_CASE("string and string_view") {
    constexpr const char* lorem =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua.";
    const args_fixture fixture({"--foo", lorem});
    CHECK(fixture.args().get<std::string>("foo") == std::string(lorem));
    CHECK(fixture.args().get<std::string_view>("foo") == lorem);
  }

  TEST_CASE("empty values") {
    const args_fixture fixture({"--foo", "", "--bar="});
    CHECK(fixture.args().get<std::string>("foo") == "");
    CHECK(fixture.args().get<std::string>("bar") == "");
    CHECK(fixture.args().get<int>("foo") == std::nullopt);
  }

  TEST_CASE("packed key=value syntax") {
    const args_fixture fixture({"--foo=bar", "---baz=qux", "--pair=a=b=c",
                                "--neg=-42", "--empty="});
    CHECK(fixture.args().get<std::string_view>("foo") == "bar");
    CHECK(fixture.args().get<std::string_view>("baz") == "qux");
    CHECK(fixture.args().get<std::string_view>("pair") == "a=b=c");
    CHECK(fixture.args().get<int>("neg") == -42);
    CHECK(fixture.args().get<std::string_view>("empty") == "");
  }

  TEST_CASE("numbers and strict numeric validation") {
    const args_fixture fixture(
        {"--int", "42", "--plus", "+42", "--neg", "-17", "--double", "42.42",
         "--neg-double", "-3.14", "--dot-neg", "-.5", "--u8", "255", "--u8-ovf",
         "256", "--u-neg", "-1", "--garbage", "42abc", "--bad-plus", "+-5",
         "--only-plus", "+"});

    CHECK(fixture.args().get<int>("int") == 42);
    CHECK(fixture.args().get<double>("int") == doctest::Approx(42.0));
    CHECK(fixture.args().get<int>("plus") == 42);
    CHECK(fixture.args().get<int>("neg") == -17);
    CHECK(fixture.args().get<double>("double") == doctest::Approx(42.42));
    CHECK(fixture.args().get<int>("double") == std::nullopt);
    CHECK(fixture.args().get<double>("neg-double") == doctest::Approx(-3.14));
    CHECK(fixture.args().get<double>("dot-neg") == doctest::Approx(-0.5));
    CHECK(fixture.args().get<std::uint8_t>("u8") == 255);
    CHECK(fixture.args().get<std::uint8_t>("u8-ovf") == std::nullopt);
    CHECK(fixture.args().get<unsigned int>("u-neg") == std::nullopt);
    CHECK(fixture.args().get<int>("garbage") == std::nullopt);
    CHECK(fixture.args().get<int>("bad-plus") == std::nullopt);
    CHECK(fixture.args().get<int>("only-plus") == std::nullopt);
    CHECK(fixture.args().get<int>("missing") == std::nullopt);
    CHECK(fixture.args().get<double>("missing") == std::nullopt);
  }

  TEST_CASE("char parsing") {
    const args_fixture fixture(
        {"--comma", ",", "--space", " ", "--multi", "ab", "--empty", ""});
    CHECK(fixture.args().get<char>("comma") == ',');
    CHECK(fixture.args().get<char>("space") == ' ');
    CHECK(fixture.args().get<char>("multi") == std::nullopt);
    CHECK(fixture.args().get<char>("empty") == std::nullopt);
  }

  TEST_CASE("dash-only and malformed option tokens do not invoke UB") {
    const args_fixture fixture({"-", "-o", "-", "---", "--=val", "pos"});
    CHECK(fixture.args().get<std::string_view>("o") == "-");
    REQUIRE(fixture.args().positional().size() == 4);
    CHECK(fixture.args().positional()[0] == "-");
    CHECK(fixture.args().positional()[1] == "---");
    CHECK(fixture.args().positional()[2] == "--=val");
    CHECK(fixture.args().positional()[3] == "pos");
    CHECK_FALSE(fixture.args().contains(""));
  }

  TEST_CASE("repeated flags with get (last wins) and get_multiple (all)") {
    const args_fixture fixture({"--foo", "bar", "--foo", "baz"});
    CHECK(fixture.args().get<std::string>("foo") == "baz");

    const auto foo = fixture.args().get_multiple<std::string>("foo");
    REQUIRE(foo.size() == 2);
    CHECK(foo[0] == "bar");
    CHECK(foo[1] == "baz");
  }

  TEST_CASE("get_multiple with valueless and malformed entries") {
    const args_fixture fixture({"-x", "-x", "1", "-x", "bad", "-x", "2"});
    const auto x = fixture.args().get_multiple<int>("x");
    REQUIRE(x.size() == 4);
    CHECK(x[0] == std::nullopt);
    CHECK(x[1] == 1);
    CHECK(x[2] == std::nullopt);
    CHECK(x[3] == 2);
  }

  TEST_CASE("get_multiple with bool falsities and valueless flags") {
    std::vector<std::string> cli;
    for (const auto falsity : flags::detail::falsities) {
      cli.emplace_back("--foo");
      cli.emplace_back(falsity);
    }
    const args_fixture fixture(cli);
    const auto foos = fixture.args().get_multiple<bool>("foo");
    REQUIRE(foos.size() == flags::detail::falsities.size());
    for (const auto& foo : foos) {
      REQUIRE(foo.has_value());
      CHECK(*foo == false);
    }

    const args_fixture valueless({"--foo", "-foo", "--foo", "-foo"});
    const auto flags_vec = valueless.args().get_multiple<bool>("foo");
    REQUIRE(flags_vec.size() == 4);
    for (const auto& flag : flags_vec) {
      REQUIRE(flag.has_value());
      CHECK(*flag == true);
    }
  }

  TEST_CASE("multi-key aliases for get and get_multiple") {
    const args_fixture fixture(
        {"--verbose", "--count", "42", "-o", "out.txt", "-I", "/a",
         "--include", "/b", "-I", "/c"});

    // First key matches
    CHECK(fixture.args().get<bool>("verbose", "v") == true);
    // Second key matches
    CHECK(fixture.args().get<int>("c", "count") == 42);
    // 3+ aliases
    CHECK(fixture.args().get<std::string>("output", "out", "o") == "out.txt");
    // Ensure string 2-key call resolves to aliases (returns optional<string>)
    const auto out = fixture.args().get<std::string>("output", "o");
    static_assert(std::is_same_v<decltype(out), const std::optional<std::string>>);
    CHECK(out == "out.txt");
    // Neither matches
    CHECK(fixture.args().get<bool>("debug", "d") == std::nullopt);

    // Mixed aliases in get_multiple preserve exact command-line order
    const auto includes =
        fixture.args().get_multiple<std::string>("include", "I");
    REQUIRE(includes.size() == 3);
    CHECK(includes[0] == "/a");
    CHECK(includes[1] == "/b");
    CHECK(includes[2] == "/c");

    // Duplicate keys in alias list are deduplicated
    const auto deduped =
        fixture.args().get_multiple<std::string>("include", "include");
    REQUIRE(deduped.size() == 1);
    CHECK(deduped[0] == "/b");

    // No match
    CHECK(fixture.args().get_multiple<int>("bar", "b").empty());
  }

  TEST_CASE("contains and keys for flag presence and validation") {
    const args_fixture fixture({"--foo", "1", "-v", "--foo", "2", "--bar"});
    CHECK(fixture.args().contains("foo"));
    CHECK(fixture.args().contains("v"));
    CHECK(fixture.args().contains("bar"));
    CHECK_FALSE(fixture.args().contains("baz"));

    CHECK(fixture.args().contains("verbose", "v"));
    CHECK_FALSE(fixture.args().contains("debug", "d"));

    const auto& keys = fixture.args().keys();
    REQUIRE(keys.size() == 3);
    CHECK(keys[0] == "foo");
    CHECK(keys[1] == "v");
    CHECK(keys[2] == "bar");
  }

  TEST_CASE("skipped tokens after double dash") {
    const args_fixture fixture(
        {"--foo", "--", "bar", "--baz", "1", "2", "3"});
    CHECK(fixture.args().get<bool>("foo") == true);
    CHECK(fixture.args().get<bool>("baz") == std::nullopt);
    const auto& skipped = fixture.args().skipped();
    REQUIRE(skipped.size() == 5);
    CHECK(skipped[0] == "bar");
    CHECK(skipped[1] == "--baz");
    CHECK(skipped[2] == "1");
    CHECK(skipped[3] == "2");
    CHECK(skipped[4] == "3");

    const args_fixture trailing_dd({"--flag", "--"});
    CHECK(trailing_dd.args().get<bool>("flag") == true);
    CHECK(trailing_dd.args().skipped().empty());
  }

  TEST_CASE("custom extractable types via operator>>") {
    const args_fixture fixture(
        {"--date", "10 11 2016", "--bad", "10 11", "--trailing",
         "10 11 2016 extra", "1 2 2025"});
    CHECK(fixture.args().get<Date>("date") == Date{10, 11, 2016});
    CHECK(fixture.args().get<Date>("bad") == std::nullopt);
    CHECK(fixture.args().get<Date>("trailing") == std::nullopt);
    CHECK(fixture.args().get<Date>(0) == Date{1, 2, 2025});
  }

  TEST_CASE("deprecated default-value overloads still accept lvalues and rvalues") {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    const args_fixture fixture({"--count", "5", "-x", "-x", "1", "hello"});
    const int default_int = 99;
    CHECK(fixture.args().get<int>("count", 0) == 5);
    CHECK(fixture.args().get<int>("missing", default_int) == 99);
    CHECK(fixture.args().get<bool>("missing_bool", true) == true);

    const auto xs = fixture.args().get_multiple<int>("x", default_int);
    REQUIRE(xs.size() == 2);
    CHECK(xs[0] == 99);
    CHECK(xs[1] == 1);

    const std::string fallback = "fallback";
    CHECK(fixture.args().get<std::string>(0, fallback) == "hello");
    CHECK(fixture.args().get<std::string>(1, fallback) == "fallback");
    CHECK(fixture.args().get<std::string>(1, "literal") == "literal");
#if defined(_MSC_VER)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
  }
}
