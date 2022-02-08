#pragma once

namespace flugzeug {

class Function;

class DeadBlockElimination {
public:
  static bool run(Function* function);
};

} // namespace flugzeug