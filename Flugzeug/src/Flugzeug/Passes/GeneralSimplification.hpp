#pragma once

namespace flugzeug {

class Function;

class GeneralSimplification {
public:
  static bool run(Function* function);
};

} // namespace flugzeug