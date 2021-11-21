#pragma once

namespace flugzeug {

class Function;

class InstructionSimplification {
public:
  static bool run(Function* function);
};

} // namespace flugzeug