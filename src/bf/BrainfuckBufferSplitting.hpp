#pragma once
#include <Flugzeug/Passes/Pass.hpp>

namespace flugzeug {
class Function;
}

namespace bf {

class BrainfuckBufferSplitting : public flugzeug::Pass<"BrainfuckBufferSplitting"> {
public:
  static bool run(flugzeug::Function* function);
};

} // namespace bf