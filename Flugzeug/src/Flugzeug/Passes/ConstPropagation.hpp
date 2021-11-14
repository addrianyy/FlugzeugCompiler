#pragma once

namespace flugzeug {

class Value;
class Instruction;
class Function;
class Block;

class ConstPropagation {
  static Value* constant_propagate(Instruction* instruction, Block*& removed_branch_target);

public:
  static bool run(Function* function);
};

} // namespace flugzeug