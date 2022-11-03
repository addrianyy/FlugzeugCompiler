#include "DebugPrint.hpp"
#include "Platform.hpp"

#ifdef PLATFORM_WINDOWS

#include <Windows.h>

void flugzeug::debug_print(std::string_view string) {
  const std::string s = std::string(string);
  OutputDebugStringA(s.c_str());
}

#else

#include "Log.hpp"

void flugzeug::debug_print(std::string_view string) {
  log_debug("{}", string);
}

#endif
