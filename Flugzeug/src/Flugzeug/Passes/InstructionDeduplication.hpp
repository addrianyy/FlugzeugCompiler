#pragma once
#include "Pass.hpp"
#include "Utils/OptimizationLocality.hpp"

namespace flugzeug::opt {

class InstructionDeduplication : public Pass<"InstructionDeduplication"> {
 public:
  static bool run(Function* function, OptimizationLocality locality);
};

}  // namespace flugzeug::opt