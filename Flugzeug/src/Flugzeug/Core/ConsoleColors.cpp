#include "ConsoleColors.hpp"
#include "Platform.hpp"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif

void console_colors::ensure_initialized() {
#ifdef PLATFORM_WINDOWS
  static bool initialized_console = false;
  if (!initialized_console) {
    const auto stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    const auto stderr_handle = GetStdHandle(STD_ERROR_HANDLE);

    DWORD console_mode;

    {
      GetConsoleMode(stdout_handle, &console_mode);
      console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(stdout_handle, console_mode);
    }

    {
      GetConsoleMode(stderr_handle, &console_mode);
      console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(stderr_handle, console_mode);
    }

    initialized_console = true;
  }
#endif
}

bool console_colors::are_allowed() {
  // TODO
  return true;
}

void console_colors::reset_color(std::ostream& stream) { stream << "\x1b[0m"; }
void console_colors::set_color(std::ostream& stream, int color) {
  stream << "\x1b[1;" << color << "m";
}