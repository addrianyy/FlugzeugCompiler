#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class KnownBitsOptimization : public Pass<"KnownBitsOptimization"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt