#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class LoopRotation : public Pass<"LoopRotation"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt