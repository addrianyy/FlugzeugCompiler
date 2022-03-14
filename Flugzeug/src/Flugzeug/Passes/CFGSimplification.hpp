#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class CFGSimplification : public Pass<"CFGSimplification"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt