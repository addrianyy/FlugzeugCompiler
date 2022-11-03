#pragma once
#include <string>

namespace flugzeug {

uint32_t run_process(const std::string& application,
                     const std::string& command_line,
                     const std::string& process_stdin);

}