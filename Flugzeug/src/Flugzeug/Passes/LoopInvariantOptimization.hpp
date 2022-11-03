#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class LoopInvariantOptimization : public Pass<"LoopInvariantOptimization"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt