#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class ConditionalFlattening : public Pass<"ConditionalFlattening"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt