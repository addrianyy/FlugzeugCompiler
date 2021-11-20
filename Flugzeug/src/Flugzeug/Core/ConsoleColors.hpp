#pragma once
#include <ostream>

namespace console_colors {

void ensure_initialized();
bool are_allowed();

void reset_color(std::ostream& stream);
void set_color(std::ostream& stream, int color);

} // namespace console_colors