#include "DebugIRPrinter.hpp"

#include <Flugzeug/Core/Platform.hpp>

using namespace flugzeug;

#ifdef PLATFORM_WINDOWS

#include <Windows.h>

static void debug_print(std::string_view string) {
  const std::string s = std::string(string);
  OutputDebugStringA(s.c_str());
}

#else

#include <Flugzeug/Core/Log.hpp>

static void debug_print(std::string_view string) {
  log_debug("{}", string);
}

#endif

void DebugIRPrinter::write_string(std::string_view string) {
  debug_print(string);
}

DebugIRPrinter::DebugIRPrinter() {
  debug_print("\n------------------------------\n");
}
