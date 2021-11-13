#pragma once

namespace flugzeug {

class Value;
class Instruction;
class Function;

class ConstPropagation {
  static Value* constant_propagate(Instruction* instruction);

public:
  static bool run(Function* function);
};

} // namespace flugzeug