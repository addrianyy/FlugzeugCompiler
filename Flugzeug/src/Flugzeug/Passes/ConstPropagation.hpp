#pragma once

namespace flugzeug {

class Function;

namespace opt {

class ConstPropagation {
public:
  static bool run(Function* function);
};

} // namespace opt

} // namespace flugzeug