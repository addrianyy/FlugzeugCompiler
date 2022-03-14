#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class ConstPropagation : public Pass<"ConstPropagation"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt