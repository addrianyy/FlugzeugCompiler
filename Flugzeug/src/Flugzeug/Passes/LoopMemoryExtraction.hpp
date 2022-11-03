#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class LoopMemoryExtraction : public Pass<"LoopMemoryExtraction"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt