#pragma once

namespace flugzeug {

class Function;

class ConstPropagation {
public:
  static bool run(Function* function);
};

} // namespace flugzeug