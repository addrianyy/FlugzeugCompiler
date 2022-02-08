#pragma once

namespace flugzeug {

class Function;

class DeadCodeElimination {
public:
  static bool run(Function* function);
};

} // namespace flugzeug