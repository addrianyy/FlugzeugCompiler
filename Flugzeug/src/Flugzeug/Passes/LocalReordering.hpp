#pragma once

namespace flugzeug {

class Function;

namespace opt {

class LocalReordering {
public:
  static bool run(Function* function);
};

} // namespace opt

} // namespace flugzeug