#include "flags.h"

#include <array>
#include <mettle.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace mettle;

namespace {
// TODO: Initialize argv into unique_ptr so RAII can take care of everything.
// Currently C-style initialization and deletion purely out of laziness. :(

// Allocates a position within argv and copies the given view to it.
// argv must already be allocated.
void initialize_arg(char** argv, const size_t index,
                    const std::string_view& view) {
  argv[index] =
      reinterpret_cast<char*>(malloc((view.size() + 1) * sizeof(char)));
  memcpy(argv[index], view.data(), view.size() + 1);
}

// Allocates all of argv from the given argument array.
// - argv[0] will always be TEST, there is no need to pass a program argument.
// - argv(0, N) will contain the passed in array.
// - argv[N] will always be NULL.
// argv's final size will be the array's size plus two due to the first and last
// conditions.
char** initialize_argv(const std::initializer_list<const char*> args) {
  char** argv =
      reinterpret_cast<char**>(malloc((args.size() + 2) * sizeof(char*)));
  initialize_arg(argv, 0, "TEST");
  std::size_t i = 0;
  for (const auto& arg : args) {
    initialize_arg(argv, ++i, arg);
  }
  argv[args.size() + 1] = NULL;
  return argv;
}

// Cleans up every item within argv, then argv itself.
void cleanup_argv(char** argv) {
  size_t index = 0;
  while (argv[index]) delete[] argv[index++];
  delete argv;
}
}  // namespace

// Simple fixture for (de)allocating argv and initalizing flags::args.
struct args_fixture {
  static args_fixture create(const std::initializer_list<const char*> args) {
    return {args.size(), initialize_argv(args)};
  }
  ~args_fixture() { cleanup_argv(argv_); }

  size_t argc() const { return argc_; }
  const flags::args& args() const { return args_; }
  std::string argv(const size_t index) const {
    if (index >= argc_) throw std::out_of_range("index larger than argc");
    return argv_[index];
  };

 private:
  args_fixture(const size_t argc, char** argv)
      : argc_(argc + 1 /* for program */), argv_(argv), args_(argc_, argv) {}
  const size_t argc_;
  char** argv_;
  const flags::args args_;
};

suite<> positional_arguments("positional arguments", [](auto& _) {
  // Lack of positional arguments.
  _.test("absence", []() {
    const auto fixture =
        args_fixture::create({"--no", "positional", "--arguments"});
    expect(fixture.args().positional().size(), equal_to(0));
    expect(fixture.args().get<int>(0), equal_to(std::nullopt));
    expect(fixture.args().get<int>(0, 3), equal_to(3));
  });

  // Testing the existence of positional arguments with no options or flags.
  _.test("basic", []() {
    const auto fixture = args_fixture::create({"positional", "arguments"});
    expect(fixture.args().positional().size(), equal_to(2));
    expect(fixture.args().positional()[0], equal_to("positional"));
    expect(fixture.args().positional()[1], equal_to("arguments"));
    // Tests for positional getters
    expect(fixture.args().get<std::string_view>(0), equal_to("positional"));
    expect(fixture.args().get<std::string_view>(1), equal_to("arguments"));
    expect(fixture.args().get<std::string_view>(2), equal_to(std::nullopt));
    expect(fixture.args().get<std::string>(0, "default"),
           equal_to("positional"));
    expect(fixture.args().get<std::string>(1, "default"),
           equal_to("arguments"));
    expect(fixture.args().get<std::string>(2, "default"), equal_to("default"));
  });

  // Adding options to the mix.
  _.test("with options", []() {
    const auto fixture = args_fixture::create(
        {"positional", "arguments", "-with", "--some", "---options"});
    expect(fixture.args().positional().size(), equal_to(2));
    expect(fixture.args().positional()[0], equal_to("positional"));
    expect(fixture.args().positional()[1], equal_to("arguments"));
    // Tests for positional getters
    expect(fixture.args().get<std::string_view>(0), equal_to("positional"));
    expect(fixture.args().get<std::string_view>(1), equal_to("arguments"));
    expect(fixture.args().get<std::string_view>(2), equal_to(std::nullopt));
    expect(fixture.args().get<std::string>(0, "default"),
           equal_to("positional"));
    expect(fixture.args().get<std::string>(1, "default"),
           equal_to("arguments"));
    expect(fixture.args().get<std::string>(2, "default"), equal_to("default"));
  });

  // Adding flags to the mix.
  _.test("with flags", []() {
    const auto fixture = args_fixture::create(
        {"--flag", "\"not positional\"", "positional", "--another-flag", "foo",
         "arguments", "--bar", "42"});
    expect(fixture.args().positional().size(), equal_to(2));
    expect(fixture.args().positional()[0], equal_to("positional"));
    expect(fixture.args().positional()[1], equal_to("arguments"));
    // Tests for positional getters
    expect(fixture.args().get<std::string_view>(0), equal_to("positional"));
    expect(fixture.args().get<std::string_view>(1), equal_to("arguments"));
    expect(fixture.args().get<std::string_view>(2), equal_to(std::nullopt));
    expect(fixture.args().get<std::string>(0, "default"),
           equal_to("positional"));
    expect(fixture.args().get<std::string>(1, "default"),
           equal_to("arguments"));
    expect(fixture.args().get<std::string>(2, "default"), equal_to("default"));
  });

  // Adding both flags and options.
  _.test("with flags and options", []() {
    const auto fixture = args_fixture::create(
        {"--flag", "\"not positional\"", "positional", "--another-flag", "foo",
         "arguments", "--bar", "42", "--some", "--options", "--foobaz"});
    expect(fixture.args().positional().size(), equal_to(2));
    expect(fixture.args().positional()[0], equal_to("positional"));
    expect(fixture.args().positional()[1], equal_to("arguments"));
    // Tests for positional getters
    expect(fixture.args().get<std::string_view>(0), equal_to("positional"));
    expect(fixture.args().get<std::string_view>(1), equal_to("arguments"));
    expect(fixture.args().get<std::string_view>(2), equal_to(std::nullopt));
    expect(fixture.args().get<std::string>(0, "default"),
           equal_to("positional"));
    expect(fixture.args().get<std::string>(1, "default"),
           equal_to("arguments"));
    expect(fixture.args().get<std::string>(2, "default"), equal_to("default"));
  });
});

suite<> flag_parsing("flag parsing", [](auto& _) {
  // Basic bool parsing.
  _.test("bool", []() {
    const auto fixture =
        args_fixture::create({"--foo", "1", "--bar", "no", "--verbose"});
    expect(*fixture.args().get<bool>("foo"), equal_to(true));
    expect(fixture.args().get<bool>("foo", false), equal_to(true));
    expect(*fixture.args().get<bool>("bar"), equal_to(false));
    expect(*fixture.args().get<bool>("verbose"), equal_to(true));
    expect(fixture.args().get<bool>("nonexistent"), equal_to(std::nullopt));
  });

  // Verifying all the falsities are actually false.
  _.test("falsities", []() {
    for (const auto falsity : flags::detail::falsities) {
      const auto fixture = args_fixture::create({"--foo", falsity});
      expect(*fixture.args().get<bool>("foo"), equal_to(false));
      expect(fixture.args().get<bool>("foo", true), equal_to(false));
    }
  });

  // Complex strings are succesfully parsed.
  _.test("string", []() {
    constexpr char LOREM_IPSUM[] =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua.";
    const auto fixture = args_fixture::create({"--foo", LOREM_IPSUM});
    expect(*fixture.args().get<std::string>("foo"),
           equal_to(std::string(LOREM_IPSUM)));
  });

  // Basic number parsing. Verifying ints are truncated and doubles are
  // succesfully parsed.
  _.test("numbers", []() {
    const auto fixture =
        args_fixture::create({"--foo", "42", "--bar", "42.42"});
    expect(*fixture.args().get<int>("foo"), equal_to(42));
    expect(*fixture.args().get<double>("foo"), equal_to(42));
    expect(*fixture.args().get<int>("bar"), equal_to(42));
    expect(*fixture.args().get<double>("bar"), equal_to(42.42));
    expect(fixture.args().get<int>("foobar", 42), equal_to(42));
    expect(fixture.args().get<double>("foobar", 42.4242), equal_to(42.4242));
    expect(fixture.args().get<int>("foobar"), equal_to(std::nullopt));
    expect(fixture.args().get<double>("foobar"), equal_to(std::nullopt));
  });
});
