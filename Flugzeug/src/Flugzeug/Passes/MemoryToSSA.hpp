#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class MemoryToSSA : public Pass<"MemoryToSSA"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt