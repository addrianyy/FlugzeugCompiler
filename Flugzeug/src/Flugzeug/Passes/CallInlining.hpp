#pragma once

namespace flugzeug {

class Function;

enum class InliningStrategy {
  InlineEverything,
};

class CallInlining {
public:
  static bool run(Function* function, InliningStrategy strategy);
};

} // namespace flugzeug