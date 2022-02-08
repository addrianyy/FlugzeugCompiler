#pragma once

namespace flugzeug {

class Function;

class ConditionalFlattening {
public:
  static bool run(Function* function);
};

} // namespace flugzeug