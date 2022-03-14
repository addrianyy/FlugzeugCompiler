#pragma once
#include <Flugzeug/Passes/Pass.hpp>

namespace flugzeug {
class Function;
}

namespace bf {

class BrainfuckLoopOptimization : public flugzeug::Pass<"BrainfuckLoopOptimization"> {
public:
  static bool run(flugzeug::Function* function);
};

} // namespace bf