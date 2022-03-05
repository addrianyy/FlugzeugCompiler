#pragma once

namespace flugzeug {

class Function;

namespace opt {

enum class InliningStrategy {
  InlineEverything,
};

class CallInlining {
public:
  static bool run(Function* function, InliningStrategy strategy);
};

} // namespace opt

} // namespace flugzeug