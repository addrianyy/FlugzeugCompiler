#pragma once
#include <fmt/format.h>
#include <string>

namespace detail::error {

[[noreturn]] void fatal_error(const char* file, int line, const std::string& message);
[[noreturn]] void assert_fail(const char* file, int line, const std::string& message);

template <typename S, typename... Args>
[[noreturn]] inline void fatal_error_fmt(const char* file, int line, const S& format,
                                         Args&&... args) {
  fatal_error(file, line, fmt::format(format, args...));
}

template <typename S, typename... Args>
inline void assert_fmt(const char* file, int line, bool value, const S& format, Args&&... args) {
  if (!value) {
    assert_fail(file, line, fmt::format(format, args...));
  }
}

} // namespace detail::error

#define fatal_error(format, ...)                                                                   \
  ::detail::error::fatal_error_fmt(__FILE__, __LINE__, (format), ##__VA_ARGS__)

#define verify(value, format, ...)                                                                 \
  ::detail::error::assert_fmt(__FILE__, __LINE__, !!(value), (format), ##__VA_ARGS__)

#define unreachable() fatal_error("Entered unreachable code.")
