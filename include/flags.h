#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <istream>
#include <locale>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace flags::detail {

template <typename T>
concept Extractable =
    std::default_initializable<T> && requires(std::istream& is, T& val) {
      { is >> val } -> std::convertible_to<std::istream&>;
    };

template <typename T>
concept CharType =
    std::same_as<T, char> || std::same_as<T, wchar_t> ||
    std::same_as<T, char8_t> || std::same_as<T, char16_t> ||
    std::same_as<T, char32_t>;

template <typename T>
concept NumericIntegral =
    std::integral<T> && !std::same_as<T, bool> && !CharType<T>;

template <typename T>
concept FromCharsParsable = requires(const char* first, const char* last,
                                     T& val) {
  std::from_chars(first, last, val);
};

// Special case for booleans: case-insensitive values considered falsy.
inline constexpr std::array<std::string_view, 5> falsities{"0", "n", "no", "f",
                                                           "false"};

[[nodiscard]] constexpr bool is_ascii_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

[[nodiscard]] constexpr char ascii_lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] constexpr bool iequals(std::string_view a,
                                     std::string_view b) noexcept {
  return a.size() == b.size() &&
         std::ranges::equal(a, b, [](char lhs, char rhs) {
           return ascii_lower(lhs) == ascii_lower(rhs);
         });
}

[[nodiscard]] constexpr bool is_negative_number(
    std::string_view token) noexcept {
  if (token.size() < 2 || token[0] != '-') return false;
  if (is_ascii_digit(token[1])) return true;
  return token[1] == '.' && token.size() >= 3 && is_ascii_digit(token[2]);
}

[[nodiscard]] constexpr std::string_view strip_leading_plus(
    std::string_view sv) noexcept {
  if (sv.size() >= 2 && sv[0] == '+' && sv[1] != '-' && sv[1] != '+') {
    sv.remove_prefix(1);
  }
  return sv;
}

struct option_value {
  std::optional<std::string_view> value;
  std::size_t order = 0;
};

using argument_map =
    std::unordered_map<std::string_view, std::vector<option_value>>;

// Non-destructively parses the argv tokens.
// * "--" stops option parsing and stores all remaining tokens in skipped().
// * A token beginning with '-' (except standalone "-", dash-only tokens,
//   empty-name "--=...", or a negative number following an option) is treated
//   as an option.
// * Otherwise, the token is assigned as the value for the preceding option, or
//   stored as a positional argument if there is no preceding option.
struct parser {
  parser(int argc, const char* const* argv)
      : parser(argc > 0 && argv != nullptr
                   ? std::span<const char* const>(
                         argv, static_cast<std::size_t>(argc))
                   : std::span<const char* const>{}) {}

  explicit parser(std::span<const char* const> argv) {
    if (argv.empty()) return;
    if (argv[0] != nullptr) program_ = argv[0];

    std::optional<std::string_view> current_option;
    std::size_t next_order = 0;

    const auto record_value =
        [&](std::optional<std::string_view> value = std::nullopt) {
          if (!current_option) {
            if (value) positional_arguments_.emplace_back(*value);
            return;
          }
          auto [it, inserted] = options_.try_emplace(*current_option);
          if (inserted) option_order_.push_back(*current_option);
          it->second.push_back(option_value{value, next_order++});
          current_option.reset();
        };

    const auto flush = [&]() {
      if (current_option) record_value();
    };

    for (std::size_t i = 1; i < argv.size(); ++i) {
      if (argv[i] == nullptr) break;
      const std::string_view token(argv[i]);

      // If this token is "--", flush any pending option and store remaining
      // tokens verbatim in skipped_tokens_.
      if (token == "--") {
        flush();
        skipped_tokens_.reserve(argv.size() - i - 1);
        for (std::size_t j = i + 1; j < argv.size(); ++j) {
          if (argv[j] == nullptr) break;
          skipped_tokens_.emplace_back(argv[j]);
        }
        break;
      }

      if (token.empty() || token == "-") {
        record_value(token);
        continue;
      }

      if (token[0] == '-') {
        // Treat negative numeric tokens (e.g. -5, -3.14, -.5) as values when
        // an option is waiting for one.
        if (current_option && is_negative_number(token)) {
          record_value(token);
          continue;
        }

        const auto first_non_dash = token.find_first_not_of('-');
        if (first_non_dash == std::string_view::npos) {
          record_value(token);
          continue;
        }

        const auto stripped = token.substr(first_non_dash);
        const auto delimiter = stripped.find('=');
        if (delimiter == 0) {
          // Malformed empty-name option (e.g. "--=value"): treat as value.
          record_value(token);
          continue;
        }

        flush();
        if (delimiter != std::string_view::npos) {
          current_option = stripped.substr(0, delimiter);
          record_value(stripped.substr(delimiter + 1));
        } else {
          current_option = stripped;
        }
      } else {
        record_value(token);
      }
    }

    // Drain any trailing valueless option.
    flush();
  }

  [[nodiscard]] std::string_view program() const noexcept { return program_; }
  [[nodiscard]] const argument_map& options() const noexcept {
    return options_;
  }
  [[nodiscard]] const std::vector<std::string_view>& keys() const noexcept {
    return option_order_;
  }
  [[nodiscard]] const std::vector<std::string_view>& positional_arguments()
      const noexcept {
    return positional_arguments_;
  }
  [[nodiscard]] const std::vector<std::string_view>& skipped_tokens()
      const noexcept {
    return skipped_tokens_;
  }

 private:
  std::string_view program_;
  argument_map options_;
  std::vector<std::string_view> option_order_;
  std::vector<std::string_view> positional_arguments_;
  std::vector<std::string_view> skipped_tokens_;
};

// Parses a single string_view token into <T>.
template <class T>
[[nodiscard]] std::optional<T> parse(std::string_view raw) {
  if constexpr (std::same_as<T, std::string_view>) {
    return raw;
  } else if constexpr (std::same_as<T, std::string>) {
    return std::string(raw);
  } else if constexpr (std::same_as<T, bool>) {
    return std::ranges::none_of(
        falsities, [raw](std::string_view falsity) {
          return iequals(raw, falsity);
        });
  } else if constexpr (std::same_as<T, char>) {
    if (raw.size() == 1) return raw[0];
    return std::nullopt;
  } else if constexpr (NumericIntegral<T>) {
    const auto sv = strip_leading_plus(raw);
    if (sv.empty()) return std::nullopt;
    T value{};
    const auto* const end = sv.data() + sv.size();
    const auto [ptr, ec] = std::from_chars(sv.data(), end, value);
    if (ec == std::errc{} && ptr == end) return value;
    return std::nullopt;
  } else if constexpr (std::floating_point<T>) {
    if constexpr (FromCharsParsable<T>) {
      const auto sv = strip_leading_plus(raw);
      if (sv.empty()) return std::nullopt;
      T value{};
      const auto* const end = sv.data() + sv.size();
      const auto [ptr, ec] = std::from_chars(sv.data(), end, value);
      if (ec == std::errc{} && ptr == end) return value;
      return std::nullopt;
    } else {
      std::istringstream iss{std::string(raw)};
      iss.imbue(std::locale::classic());
      if (T value{}; (iss >> value) && (iss >> std::ws).eof()) return value;
      return std::nullopt;
    }
  } else {
    static_assert(
        Extractable<T>,
        "T must be default-initializable and support extraction via "
        "operator>>");
    std::istringstream iss{std::string(raw)};
    if (T value{}; (iss >> value) && (iss >> std::ws).eof()) return value;
    return std::nullopt;
  }
}

// Parses a stored option entry's optional raw value into std::optional<T>.
// For booleans, a present option without an explicit value evaluates to true.
template <class T>
[[nodiscard]] std::optional<T> parse_option_value(
    const std::optional<std::string_view>& raw) {
  if constexpr (std::same_as<T, bool>) {
    return raw ? parse<bool>(*raw) : std::optional<bool>(true);
  } else {
    return raw.and_then([](std::string_view sv) { return parse<T>(sv); });
  }
}

// Coerces the last-supplied value of the given option into <T>.
// If the key does not exist or its value cannot be parsed, returns nullopt.
template <class T>
[[nodiscard]] std::optional<T> get(const argument_map& options,
                                   std::string_view option) {
  const auto it = options.find(option);
  if (it == options.end()) return std::nullopt;
  return parse_option_value<T>(it->second.back().value);
}

// Coerces all values of the given option into std::vector<std::optional<T>>.
// Missing or unparseable values produce std::nullopt entries (except for bool,
// where a valueless flag produces true). Returns an empty vector if the key
// does not exist.
template <class T>
[[nodiscard]] std::vector<std::optional<T>> get_multiple(
    const argument_map& options, std::string_view option) {
  const auto it = options.find(option);
  if (it == options.end()) return {};

  std::vector<std::optional<T>> values;
  values.reserve(it->second.size());
  for (const auto& entry : it->second) {
    values.push_back(parse_option_value<T>(entry.value));
  }
  return values;
}

// Coerces all values across any of the given alias keys in command-line order.
template <class T>
[[nodiscard]] std::vector<std::optional<T>> get_multiple(
    const argument_map& options, std::initializer_list<std::string_view> keys) {
  std::vector<const option_value*> matched;
  std::vector<std::string_view> seen;
  seen.reserve(keys.size());

  for (const auto key : keys) {
    if (std::ranges::find(seen, key) != seen.end()) continue;
    seen.push_back(key);
    if (const auto it = options.find(key); it != options.end()) {
      matched.reserve(matched.size() + it->second.size());
      for (const auto& entry : it->second) {
        matched.push_back(&entry);
      }
    }
  }

  std::ranges::sort(matched, {}, &option_value::order);

  std::vector<std::optional<T>> values;
  values.reserve(matched.size());
  for (const auto* entry : matched) {
    values.push_back(parse_option_value<T>(entry->value));
  }
  return values;
}

// Coerces the string value of the given positional index into <T>.
// If the value cannot be parsed or the index is out of range, returns nullopt.
template <class T>
[[nodiscard]] std::optional<T> get(
    const std::vector<std::string_view>& positional_arguments,
    std::size_t positional_index) {
  if (positional_index < positional_arguments.size()) {
    return parse<T>(positional_arguments[positional_index]);
  }
  return std::nullopt;
}

}  // namespace flags::detail

namespace flags {

// Non-owning command-line argument parser. The backing argv buffers must
// outlive this object.
struct args {
  args(int argc, const char* const* argv) : parser_(argc, argv) {}
  explicit args(std::span<const char* const> argv) : parser_(argv) {}

  [[nodiscard]] std::string_view program() const noexcept {
    return parser_.program();
  }

  [[nodiscard]] bool contains(std::string_view option) const {
    return parser_.options().contains(option);
  }

  template <typename... Keys>
    requires(sizeof...(Keys) >= 2 &&
             (std::convertible_to<Keys, std::string_view> && ...))
  [[nodiscard]] bool contains(Keys... keys) const {
    return (contains(std::string_view(keys)) || ...);
  }

  template <class T>
  [[nodiscard]] std::optional<T> get(std::string_view option) const {
    return detail::get<T>(parser_.options(), option);
  }

  template <class T, class U = T>
    requires(!std::convertible_to<U, std::string_view> &&
             std::convertible_to<U&&, T>)
  [[deprecated("Use get<T>(option).value_or(default_value) instead")]]
  [[nodiscard]] T get(std::string_view option, U&& default_value) const {
    return get<T>(option).value_or(std::forward<U>(default_value));
  }

  template <class T, typename... Keys>
    requires(sizeof...(Keys) >= 2 &&
             (std::convertible_to<Keys, std::string_view> && ...))
  [[nodiscard]] std::optional<T> get(Keys... keys) const {
    std::optional<T> result;
    ((result = get<T>(std::string_view(keys)), result.has_value()) || ...);
    return result;
  }

  template <class T>
  [[nodiscard]] std::vector<std::optional<T>> get_multiple(
      std::string_view option) const {
    return detail::get_multiple<T>(parser_.options(), option);
  }

  template <class T, class U = T>
    requires(!std::convertible_to<U, std::string_view> &&
             std::convertible_to<U&&, T>)
  [[deprecated(
      "Use get_multiple<T>(option) and handle std::nullopt directly instead")]]
  [[nodiscard]] std::vector<T> get_multiple(std::string_view option,
                                            U&& default_value) const {
    const auto items = get_multiple<T>(option);
    const T fallback(std::forward<U>(default_value));
    std::vector<T> values;
    values.reserve(items.size());
    for (const auto& item : items) {
      values.push_back(item.value_or(fallback));
    }
    return values;
  }

  template <class T, typename... Keys>
    requires(sizeof...(Keys) >= 2 &&
             (std::convertible_to<Keys, std::string_view> && ...))
  [[nodiscard]] std::vector<std::optional<T>> get_multiple(Keys... keys) const {
    return detail::get_multiple<T>(parser_.options(),
                                   {std::string_view(keys)...});
  }

  template <class T>
  [[nodiscard]] std::optional<T> get(std::size_t positional_index) const {
    return detail::get<T>(parser_.positional_arguments(), positional_index);
  }

  template <class T, class U = T>
    requires std::convertible_to<U&&, T>
  [[deprecated("Use get<T>(positional_index).value_or(default_value) instead")]]
  [[nodiscard]] T get(std::size_t positional_index, U&& default_value) const {
    return get<T>(positional_index).value_or(std::forward<U>(default_value));
  }

  [[nodiscard]] const std::vector<std::string_view>& keys() const noexcept {
    return parser_.keys();
  }

  [[nodiscard]] const std::vector<std::string_view>& positional()
      const noexcept {
    return parser_.positional_arguments();
  }

  [[nodiscard]] const std::vector<std::string_view>& skipped() const noexcept {
    return parser_.skipped_tokens();
  }

 private:
  detail::parser parser_;
};

}  // namespace flags
