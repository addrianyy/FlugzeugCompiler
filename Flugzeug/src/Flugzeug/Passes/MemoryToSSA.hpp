#pragma once

namespace flugzeug {

class Function;

class MemoryToSSA {
public:
  static bool run(Function* function);
};

} // namespace flugzeug