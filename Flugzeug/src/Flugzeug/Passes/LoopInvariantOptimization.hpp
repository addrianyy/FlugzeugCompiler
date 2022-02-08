#pragma once

namespace flugzeug {

class Function;

class LoopInvariantOptimization {
public:
  static bool run(Function* function);
};

} // namespace flugzeug