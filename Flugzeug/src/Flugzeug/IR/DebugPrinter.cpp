#include "DebugPrinter.hpp"
#include <Flugzeug/Core/DebugPrint.hpp>

using namespace flugzeug;

void DebugPrinter::write_string(std::string_view string) { debug_print(string); }