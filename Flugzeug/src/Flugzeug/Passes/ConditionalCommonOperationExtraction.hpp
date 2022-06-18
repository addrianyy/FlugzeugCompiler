#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

class ConditionalCommonOperationExtraction : public Pass<"ConditionalCommonOperationExtraction"> {
public:
  static bool run(Function* function);
};

} // namespace flugzeug::opt