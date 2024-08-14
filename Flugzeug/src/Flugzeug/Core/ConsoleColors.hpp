#pragma once
#include <ostream>

namespace flugzeug {

class ConsoleColors {
 public:
  static void ensure_initialized();
  static bool supported();

  static void reset_color(std::ostream& stream);
  static void set_color(std::ostream& stream, int color);
};

}  // namespace flugzeug