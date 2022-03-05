#pragma once

namespace flugzeug {

class Function;

namespace opt {

class DeadCodeElimination {
public:
  static bool run(Function* function);
};

} // namespace opt

} // namespace flugzeug