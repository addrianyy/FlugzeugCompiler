#pragma once

namespace flugzeug {
class Function;
}

namespace bf {

class BrainfuckLoopOptimization {
public:
  static bool run(flugzeug::Function* function);
};

} // namespace bf