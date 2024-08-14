#include "Error.hpp"
#include "ConsoleColors.hpp"
#include "Log.hpp"

#include <atomic>
#include <cstdlib>
#include <thread>

static std::atomic_bool g_is_panicking = false;

#undef fatal_error
#undef verify
#undef unreachable

[[noreturn]] void flugzeug::detail::error::fatal_error(const char* file,
                                                       int line,
                                                       const std::string& message) {
  if (g_is_panicking.exchange(true)) {
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
  }

  ConsoleColors::ensure_initialized();

  log_error("{}:{} => {}", file, line, message);

  // For debugger breakpoint:
  {
    int _unused = 0;
    (void)_unused;
  }

  std::exit(1);
}

[[noreturn]] void flugzeug::detail::error::assert_fail(const char* file,
                                                       int line,
                                                       const std::string& message) {
  if (message.empty()) {
    fatal_error(file, line, "Assertion failed.");
  } else {
    fatal_error(file, line, fmt::format("Assertion failed: {}.", message));
  }
}