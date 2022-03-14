#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class LoopUnrolling : public Pass<"LoopUnrolling"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt