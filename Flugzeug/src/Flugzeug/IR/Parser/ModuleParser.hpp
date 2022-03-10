#pragma once
#include <string>

namespace flugzeug {

class Module;

void parse_to_module(std::string source, Module* module);

} // namespace flugzeug