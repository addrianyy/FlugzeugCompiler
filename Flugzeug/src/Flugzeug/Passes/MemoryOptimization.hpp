#pragma once
#include "Pass.hpp"
#include "Utils/OptimizationLocality.hpp"

namespace flugzeug::opt {

class MemoryOptimization : public Pass<"MemoryOptimization"> {
public:
  static bool run(Function* function, OptimizationLocality locality);
};

} // namespace flugzeug::opt