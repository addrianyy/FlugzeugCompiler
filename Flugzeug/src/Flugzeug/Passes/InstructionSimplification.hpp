#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class InstructionSimplification : public Pass<"InstructionSimplification"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt