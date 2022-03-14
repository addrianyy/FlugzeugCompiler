#pragma once
#include "Pass.hpp"

namespace flugzeug::opt {

enum class InliningStrategy {
  InlineEverything,
};

class CallInlining : public Pass<"CallInlining"> {
public:
  static bool run(Function* function, InliningStrategy strategy);
};

} // namespace flugzeug::opt