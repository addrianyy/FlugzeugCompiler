#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class LocalReordering : public Pass<"LocalReordering"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt