#include "Error.hpp"
#include "Log.hpp"
#include "Platform.hpp"
#include <cstdlib>

#undef fatal_error
#undef verify
#undef unreachable

#ifdef PLATFORM_WINDOWS

#include <Windows.h>

#endif

[[noreturn]] void detail::error::fatal_error(const char* file, int line,
                                             const std::string& message) {
  log_error("{}:{} => {}", file, line, message);

#ifdef PLATFORM_WINDOWS
  if (message.size() < 128) {
    MessageBoxA(nullptr, message.c_str(), "Error", MB_ICONERROR);
  } else {
    MessageBoxA(nullptr, "Encountered fatal error.", "Error", MB_ICONERROR);
  }

  if (IsDebuggerPresent()) {
    // DebugBreak();
  }
#endif

  std::exit(-1);
}

[[noreturn]] void detail::error::assert_fail(const char* file, int line,
                                             const std::string& message) {
  if (message.empty()) {
    fatal_error(file, line, "Assertion failed.");
  } else {
    fatal_error(file, line, fmt::format("Assertion failed: {}.", message));
  }
}