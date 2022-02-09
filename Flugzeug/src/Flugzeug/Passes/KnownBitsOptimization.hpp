#pragma once

namespace flugzeug {

class Function;

class KnownBitsOptimization {
public:
  static bool run(Function* function);
};

} // namespace flugzeug