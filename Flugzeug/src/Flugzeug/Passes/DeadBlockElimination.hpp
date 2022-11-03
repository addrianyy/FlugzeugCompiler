#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class DeadBlockElimination : public Pass<"DeadBlockElimination"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt