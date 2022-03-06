#pragma once
#include <Flugzeug/IR/Module.hpp>

namespace turboc {

class Compiler {
public:
  static flugzeug::Module* compile_from_file(flugzeug::Context* context,
                                             const std::string& source_path);
};

} // namespace turboc