#include "Log.hpp"
#include "ConsoleColors.hpp"

#include <string_view>

#define TERMINAL_RESET_SEQUENCE "\x1b[0m"

#define TERMINAL_COLOR_RED(x) "\x1b[31;1m" x TERMINAL_RESET_SEQUENCE
#define TERMINAL_COLOR_YELLOW(x) "\x1b[33;1m" x TERMINAL_RESET_SEQUENCE
#define TERMINAL_COLOR_BLUE(x) "\x1b[34;1m" x TERMINAL_RESET_SEQUENCE
#define TERMINAL_COLOR_MAGENTA(x) "\x1b[35;1m" x TERMINAL_RESET_SEQUENCE

void flugzeug::detail::log::log(const char* file,
                                int line,
                                LogLevel level,
                                const std::string& message) {
  (void)file;
  (void)line;

  ConsoleColors::ensure_initialized();

  std::string_view header;

  switch (level) {
    case LogLevel::Debug:
      header = TERMINAL_COLOR_BLUE("[debug] ");
      break;
    case LogLevel::Info:
      header = TERMINAL_COLOR_MAGENTA("[info ] ");
      break;
    case LogLevel::Warn:
      header = TERMINAL_COLOR_YELLOW("[warn ] ");
      break;
    case LogLevel::Error:
      header = TERMINAL_COLOR_RED("[warn ] ");
      break;
    default:
      header = "??? ";
  }

  fmt::println("{}" TERMINAL_RESET_SEQUENCE "{}", header, message);
}