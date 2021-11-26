#pragma once

namespace flugzeug {

class Function;

class LocalReordering {
public:
  static bool run(Function* function);
};

} // namespace flugzeug