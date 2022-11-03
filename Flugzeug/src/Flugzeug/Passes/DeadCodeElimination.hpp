#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class DeadCodeElimination : public Pass<"DeadCodeElimination"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt