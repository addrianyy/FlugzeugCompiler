#pragma once
#include <Flugzeug/Passes/Pass.hpp>

namespace flugzeug {
class Function;
}

namespace bf {

class BrainfuckDeadBufferElimination : public flugzeug::Pass<"BrainfuckDeadBufferElimination"> {
public:
  static bool run(flugzeug::Function* function);
};

} // namespace bf