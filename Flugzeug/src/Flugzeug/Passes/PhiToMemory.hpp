#pragma once

namespace flugzeug {

class Function;
class Phi;

class PhiToMemory {
  static void convert_phi_to_memory(Phi* phi);

public:
  static bool run(Function* function);
};

} // namespace flugzeug