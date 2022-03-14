#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class PhiToMemory : public Pass<"PhiToMemory"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt