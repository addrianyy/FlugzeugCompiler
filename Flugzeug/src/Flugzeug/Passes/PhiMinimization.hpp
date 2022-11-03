#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class PhiMinimization : public Pass<"PhiMinimization"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt