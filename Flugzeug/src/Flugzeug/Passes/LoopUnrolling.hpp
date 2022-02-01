#pragma once

namespace flugzeug {

class Function;

class LoopUnrolling {
public:
  static bool run(Function* function);
};

} // namespace flugzeug