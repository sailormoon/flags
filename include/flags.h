#ifndef FLAGS_H_
#define FLAGS_H_

#include <algorithm>
#include <array>
#include <deque>
#include <experimental/optional>
#include <experimental/string_view>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flags {
using std::experimental::nullopt;
using std::experimental::optional;
using std::experimental::string_view;
using argument_map = std::unordered_map<string_view, optional<string_view>>;

namespace detail {
// Non-destructively parses the argv tokens.
// * If the token begins with a -, it will be considered an option.
// * If the token does not begin with a -, it will be considered a value for the
//   previous option. If there was no previous option, it will be considered a
//   positional argument.
struct parser {
  parser(const int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
      churn(argv[i]);
    }
    // If the last token was an option, it needs to be drained.
    flush();
  }

  const argument_map& options() const { return options_; }
  const std::deque<string_view>& positional_arguments() const {
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
  std::deque<string_view> positional_arguments_;
};

// If a key exists, return an optional populated with its value.
inline optional<string_view> get_value(const argument_map& options,
                                       const string_view& option) {
  const auto it = options.find(option);
  return it == options.end() ? optional<string_view>(nullopt) : it->second;
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
constexpr std::array<const char*, 5> falsities{{"0", "n", "no", "f", "false"}};
template <>
optional<bool> get(const argument_map& options, const string_view& option) {
  if (const auto value = get_value(options, option)) {
    return std::none_of(
        falsities.begin(), falsities.end(),
        [&value](const auto& falsity) { return *value == falsity; });
  }
  return nullopt;
}

// Forms the logical conjunction of the type traits B...
template <class...>
struct conjunction : std::true_type {};
template <class B1>
struct conjunction<B1> : B1 {};
template <class B1, class... Bn>
struct conjunction<B1, Bn...>
    : std::conditional_t<bool(B1::value), conjunction<Bn...>, B1> {};

template <class... Ts>
std::vector<std::string> variadic_string(Ts... ts) {
  static_assert(conjunction<std::is_constructible<std::string, Ts>...>::value,
                "Not all arguments were constructible to string_view.");
  return {ts...};
}

struct aliases {
  void add(const string_view& primary,
           const std::vector<std::string>::const_iterator begin,
           const std::vector<std::string>::const_iterator end) {
    auto& primary_values = extensions_[primary.to_string()];
    primary_values.emplace_back(primary.to_string());
    for (auto it = begin; it != end; ++it) {
      primary_values.emplace_back(*it);
      alias_to_primary_.emplace(*it, primary);
    }
  }

  void add(const string_view& primary,
           const std::vector<std::string>& aliases) {
    add(primary, aliases.begin(), aliases.end());
  }

  // Get's the primary key which the alias is for.
  optional<std::string> get_primary(const string_view& key) const {
    const auto it = alias_to_primary_.find(key.to_string());
    if (it == alias_to_primary_.end()) {
      return nullopt;
    }
    return {it->second};
  }

  // Iterates over the aliases for the given key until one has a satisfactory
  // value or none are found.
  template <class Callback>
  void for_each(const string_view& key, Callback&& callback) const {
    if (!callback(key)) return;
    if (const auto primary = get_primary(key)) {
      const auto it = extensions_.find(*primary);
      for (const auto& alias : it->second) {
        if (!callback(alias)) return;
      }
    }
  }

  // Gets all the underlying aliasesfor the given key.
  std::deque<std::string> get(const std::string& key) {
    const auto it = extensions_.find(key);
    if (it == extensions_.end()) {
      static const std::deque<std::string> EMPTY;
      return EMPTY;
    }
    return it->second;
  }

 private:
  std::unordered_map<std::string, std::deque<std::string>> extensions_;
  std::unordered_map<std::string, std::string> alias_to_primary_;
};

// Saves a stream's fmtflag state and restores it upon destruction.
struct stream_state_saver {
  explicit stream_state_saver(std::ios& stream)
      : stream_(stream), fmtflags_(stream_.flags()) {}
  ~stream_state_saver() { stream_.flags(fmtflags_); }

 private:
  std::ios& stream_;
  const std::ios::fmtflags fmtflags_;
};

// Validates a parser's inputs to the given schema.
struct validator {
  validator(const string_view& program_name,
            const string_view& program_description, const parser& parser,
            aliases& aliases)
      : program_name_(program_name),
        program_description_(program_description),
        parser_(parser),
        aliases_(aliases) {}

  template <class T, class... Ts>
  validator& require(const string_view& option, Ts... ts) {
    add_help<T>(option, true, ts...);
    return *this;
  }

  template <class T, class... Ts>
  validator& optional(const string_view& option, Ts... ts) {
    add_help<T>(option, false, ts...);
    return *this;
  }

  void print_help(std::ostream& out) const {
    const stream_state_saver save(out);
    out << "usage: " << program_name_;
    std::size_t longest = 0;
    for (const auto& help : help_) {
      longest = std::max(longest, help.option.size() + 1);
      if (help.required) {
        out << " --" << help.option << "=arg";
      } else {
        out << " [" << help.option << "]";
      }
    }
    if (!program_description_.empty()) {
      out << std::endl << std::endl << program_description_;
    }
    if (!help_.empty()) {
      out << std::endl << std::endl << "options: " << std::endl;
      for (const auto& help : help_) {
        out << "  --" << std::left << std::setw(longest) << help.option << " "
            << help.description << std::endl;
      }
    }
  }

  void print_and_exit_on_failure(std::ostream& out = std::cout) const {
    if (!std::all_of(help_.begin(), help_.end(),
                     [&](const auto& help) { return help.valid; })) {
      print_help(out);
      exit(EXIT_FAILURE);
    }
  }

 private:
  struct Help {
    Help() = delete;
    const std::string option;
    const std::string description;
    const bool required;
    const bool valid;
  };

  template <class T, class... Ts>
  void add_help(const string_view& option, const bool required, Ts... ts) {
    const auto strings = variadic_string(ts...);
    aliases_.add(option, strings.begin(), std::prev(strings.end()));
    help_.emplace_back(Help{option.to_string(), strings.back(), required,
                            validate<T>(option, required)});
  }

  template <class T>
  bool validate(const string_view& option, const bool required) {
    if (!required &&
        parser_.options().find(option) == parser_.options().end()) {
      // If an argument is optional and it's not present, it's valid.
      return true;
    }
    return static_cast<bool>(detail::get<T>(parser_.options(), option));
  }

  const std::string program_name_;
  const std::string program_description_;
  const parser& parser_;
  aliases& aliases_;
  bool valid_ = true;
  std::deque<Help> help_;
};

}  // namespace detail

struct args {
  args(const int argc, char** argv) : parser_(argc, argv) {}

  template <class T>
  optional<T> get(const string_view& option) const {
    return first_value<T>(option);
  }

  template <class T>
  T get(const string_view& option, T&& default_value) const {
    return get<T>(option).value_or(default_value);
  }

  template <class... Ts>
  args& alias(const string_view& primary, Ts... rest) {
    aliases_.add(primary, detail::variadic_string(rest...));
    return *this;
  }

  const std::deque<string_view>& positional() const {
    return parser_.positional_arguments();
  }

  detail::validator validate(const string_view& program_name,
                             const string_view& program_description = "") {
    return detail::validator(program_name, program_description, parser_,
                             aliases_);
  }

 private:
  // Gets the first available value from the list of aliases in the order they
  // were given in.
  template <class T>
  optional<T> first_value(const string_view& primary) const {
    optional<T> last_seen;
    aliases_.for_each(primary, [&](const auto& alias) {
      last_seen = detail::get<T>(parser_.options(), alias);
      return !last_seen;
    });
    return last_seen;
  }
  detail::aliases aliases_;
  const detail::parser parser_;
};

}  // namespace flags

#endif  // FLAGS_H_
