#pragma once
#include "Utils/OptimizationLocality.hpp"

namespace flugzeug {

class Function;

namespace opt {

class InstructionDeduplication {
public:
  static bool run(Function* function, OptimizationLocality locality);
};

} // namespace opt

} // namespace flugzeug