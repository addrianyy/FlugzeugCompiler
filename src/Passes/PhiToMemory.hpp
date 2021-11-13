#pragma once

namespace flugzeug {

class StackAlloc;
class Function;
class Block;
class Value;
class Phi;

class PhiToMemory {
  static void convert_phi_to_memory(Phi* phi);

public:
  static bool run(Function* function);
};

} // namespace flugzeug