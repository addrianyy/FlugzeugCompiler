#pragma once
#include "Utils/OptimizationLocality.hpp"

namespace flugzeug {

class Function;

namespace opt {

class KnownLoadElimination {
public:
  static bool run(Function* function, OptimizationLocality locality);
};

} // namespace opt

} // namespace flugzeug