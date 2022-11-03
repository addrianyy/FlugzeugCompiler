#pragma once
#include <fmt/core.h>
#include <string>

enum class LogLevel {
  Debug,
  Info,
  Warn,
  Error,
};

namespace flugzeug::detail::log {

void log(const char* file, int line, LogLevel level, const std::string& message);

template <typename... Args>
inline void log_fmt(const char* file,
                    int line,
                    LogLevel level,
                    fmt::format_string<Args...> fmt,
                    Args&&... args) {
  log(file, line, level, fmt::format(fmt, std::forward<Args>(args)...));
}

}  // namespace flugzeug::detail::log

#define log_message(level, format, ...) \
  ::flugzeug::detail::log::log_fmt(__FILE__, __LINE__, (level), (format), ##__VA_ARGS__)

#define log_debug(format, ...) log_message(LogLevel::Debug, (format), ##__VA_ARGS__)
#define log_info(format, ...) log_message(LogLevel::Info, (format), ##__VA_ARGS__)
#define log_warn(format, ...) log_message(LogLevel::Warn, (format), ##__VA_ARGS__)
#define log_error(format, ...) log_message(LogLevel::Error, (format), ##__VA_ARGS__)