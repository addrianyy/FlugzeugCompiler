#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class BlockInvariantPropagation : public Pass<"BlockInvariantPropagation"> {
 public:
  static bool run(Function* function);
};

}  // namespace flugzeug::opt