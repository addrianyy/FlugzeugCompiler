#pragma once

namespace flugzeug {

class Function;

class CFGSimplification {
public:
  static bool run(Function* function);
};

} // namespace flugzeug