#ifndef FLAGS_H_
#define FLAGS_H_

#include <algorithm>
#include <array>
#include <experimental/array>
#include <experimental/optional>
#include <experimental/string_view>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace flags {
using std::experimental::make_array;
using std::experimental::nullopt;
using std::experimental::optional;
using std::experimental::string_view;
using argument_map = std::unordered_map<string_view, optional<string_view>>;

namespace detail {
// Non-destructively parses the argv tokens.
// * If the token begins with a -, it will be considered an option.
// * If the token does not begin with a -, it will be considered a value for the
// previous option. If there was no previous option, it will be considered a
// positional argument.
struct parser {
  parser(const int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
      churn(argv[i]);
    }
    // If the last token was an option, it needs to be drained.
    flush();
  }
  parser& operator=(const parser&) = delete;

  const argument_map& options() const { return options_; }
  const std::vector<string_view>& positional_arguments() const {
    return positional_arguments_;
  }

 private:
  // Advance the state machine for the current token.
  void churn(const string_view& item) {
    item.at(0) == '-' ? on_option(item) : on_value(item);
  }

  // Consumes the current option if there is one.
  void flush() {
    if (current_option_) on_value();
  }

  void on_option(const string_view& option) {
    // Consume the current_option and reassign it to the new option while
    // removing all leading dashes.
    flush();
    current_option_ = option;
    current_option_->remove_prefix(current_option_->find_first_not_of('-'));

    // Handle a packed argument (--arg_name=value).
    const auto delimiter = current_option_->find_first_of('=');
    if (delimiter != string_view::npos) {
      auto value = *current_option_;
      value.remove_prefix(delimiter + 1 /* skip '=' */);
      current_option_->remove_suffix(current_option_->size() - delimiter);
      on_value(value);
    }
  }

  void on_value(const optional<string_view>& value = nullopt) {
    // If there's not an option preceding the value, it's a positional argument.
    if (!current_option_) {
      positional_arguments_.emplace_back(*value);
      return;
    }
    // Consume the preceding option and assign its value.
    options_.emplace(*current_option_, value);
    current_option_ = nullopt;
  }

  optional<string_view> current_option_;
  argument_map options_;
  std::vector<string_view> positional_arguments_;
};

// If a key exists, return an optional populated with its value.
inline optional<string_view> get_value(const argument_map& options,
                                       const string_view& option) {
  const auto it = options.find(option);
  return it != options.end() ? make_optional(*it->second) : nullopt;
}

// Coerces the string value of the given option into <T>.
// If the value cannot be properly parsed or the key does not exist, returns
// nullopt.
template <class T>
optional<T> get(const argument_map& options, const string_view& option) {
  if (const auto view = get_value(options, option)) {
    T value;
    if (std::istringstream(view->to_string()) >> value) return value;
  }
  return nullopt;
}

// Since the values are already stored as strings, there's no need to use `>>`.
template <>
optional<string_view> get(const argument_map& options,
                          const string_view& option) {
  return get_value(options, option);
}

template <>
optional<std::string> get(const argument_map& options,
                          const string_view& option) {
  if (const auto view = get<string_view>(options, option)) {
    return view->to_string();
  }
  return nullopt;
}

// Special case for booleans: if the value is any of the below, the option will
// be considered falsy. Otherwise, it will be considered truthy just for being
// present.
constexpr auto falsities = make_array("0", "n", "no", "f", "false");
template <>
optional<bool> get(const argument_map& options, const string_view& option) {
  if (const auto value = get_value(options, option)) {
    return std::none_of(falsities.begin(), falsities.end(),
                        [&value](auto falsity) { return *value == falsity; });
  }
  return nullopt;
}
}  // namespace detail

struct args {
  args(const int argc, char** argv) : parser_(argc, argv) {}

  template <class T>
  optional<T> get(const string_view& option) const {
    return detail::get<T>(parser_.options(), option);
  }

  template <class T>
  T get(const string_view& option, T&& default_value) const {
    return get<T>(option).value_or(default_value);
  }

  const std::vector<string_view>& positional() const {
    return parser_.positional_arguments();
  }

 private:
  const detail::parser parser_;
};

}  // namespace flags

#endif  // FLAGS_H_
