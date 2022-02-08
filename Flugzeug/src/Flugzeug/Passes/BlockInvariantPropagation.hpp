#pragma once

namespace flugzeug {

class Function;

class BlockInvariantPropagation {
public:
  static bool run(Function* function);
};

} // namespace flugzeug