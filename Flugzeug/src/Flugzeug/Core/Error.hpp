#pragma once
#include <fmt/core.h>
#include <string>

namespace flugzeug::detail::error {

[[noreturn]] void fatal_error(const char* file, int line, const std::string& message);
[[noreturn]] void assert_fail(const char* file, int line, const std::string& message);

template <typename... Args>
[[noreturn]] inline void fatal_error_fmt(const char* file, int line,
                                         fmt::format_string<Args...> fmt, Args&&... args) {
  fatal_error(file, line, fmt::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void assert_fmt(const char* file, int line, bool value, fmt::format_string<Args...> fmt,
                       Args&&... args) {
  if (!value) {
    assert_fail(file, line, fmt::format(fmt, std::forward<Args>(args)...));
  }
}

} // namespace flugzeug::detail::error

#define fatal_error(format, ...)                                                                   \
  ::flugzeug::detail::error::fatal_error_fmt(__FILE__, __LINE__, (format), ##__VA_ARGS__)

#define verify(value, format, ...)                                                                 \
  ::flugzeug::detail::error::assert_fmt(__FILE__, __LINE__, !!(value), (format), ##__VA_ARGS__)

#define unreachable() fatal_error("Entered unreachable code.")
