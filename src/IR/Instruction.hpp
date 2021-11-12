#pragma once
#include "IRPrinter.hpp"
#include "User.hpp"
#include <Core/IntrusiveLinkedList.hpp>

namespace flugzeug {

class Block;
class IRPrinter;

class Instruction : public User, public IntrusiveNode<Instruction, Block> {
  DEFINE_INSTANCEOF_RANGE(Value, Value::Kind::InstructionBegin, Value::Kind::InstructionEnd)

  friend class Block;

protected:
  using User::User;

  virtual void print_instruction_internal(IRPrinter::LinePrinter& printer) const = 0;

public:
  void print(IRPrinter& printer) const;

  Block* get_block() { return get_owner(); }
  const Block* get_block() const { return get_owner(); }

  void replace_instruction(Instruction* instruction);
  void destroy();

  void replace_uses_and_destroy(Value* new_value) {
    replace_uses(new_value);
    destroy();
  }

  void replace_uses_with_constant_and_destroy(uint64_t constant) {
    replace_uses_with_constant(constant);
    destroy();
  }
};

} // namespace flugzeug