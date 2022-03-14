#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class GlobalReordering : public Pass<"GlobalReordering"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt