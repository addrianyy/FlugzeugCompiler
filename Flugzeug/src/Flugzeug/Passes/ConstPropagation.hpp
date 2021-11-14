#pragma once

namespace flugzeug {

class Value;
class Instruction;
class Function;
class Block;

class ConstPropagation {
  static Value* constant_propagate(Instruction* instruction, bool& did_something);

public:
  static bool run(Function* function);
};

} // namespace flugzeug