#include "Instruction.hpp"
#include "Block.hpp"
#include "Instructions.hpp"

using namespace flugzeug;

template <typename TBlock, typename TInstruction>
BlockTargets<TBlock> get_targets_generic(TInstruction* instruction) {
  BlockTargets<TBlock> result;
  if (const auto bcond = cast<CondBranch>(instruction)) {
    const auto true_target = bcond->get_true_target();
    const auto false_target = bcond->get_false_target();

    result.insert(true_target);

    if (true_target != false_target) {
      result.insert(false_target);
    }
  } else if (const auto branch = cast<Branch>(instruction)) {
    result.insert(branch->get_target());
  }

  return result;
}

void Instruction::print(IRPrinter& printer) const {
  auto p = printer.create_line_printer();
  if (!is_void()) {
    p.print(this, IRPrinter::Item::Equals);
  }

  print_instruction_internal(p);
}

Function* Instruction::get_function() {
  return get_block() ? get_block()->get_function() : nullptr;
}

const Function* Instruction::get_function() const {
  return get_block() ? get_block()->get_function() : nullptr;
}

void Instruction::destroy() {
  if (!is_void()) {
    replace_uses_with_undef();
  }
  IntrusiveNode::destroy();
}

void Instruction::destroy_if_unused() {
  if (get_user_count_excluding_self() == 0) {
    destroy();
  }
}

void Instruction::replace_instruction_and_destroy(Instruction* instruction) {
  verify(instruction != this, "Cannot replace instruction with itself");

  instruction->insert_after(this);
  replace_uses(instruction);
  destroy();
}

void Instruction::replace_uses_with_constant_and_destroy(uint64_t constant) {
  replace_uses_with_constant(constant);
  destroy();
}

void Instruction::replace_uses_and_destroy(Value* new_value) {
  verify(new_value != this, "Cannot replace instruction with itself");

  replace_uses(new_value);
  destroy();
}

void Instruction::replace_instruction_or_uses_and_destroy(Value* new_value) {
  verify(new_value != this, "Cannot replace instruction with itself");

  const auto other = cast<Instruction>(new_value);
  if (other && !other->get_block()) {
    replace_instruction_and_destroy(other);
  } else {
    replace_uses_and_destroy(new_value);
  }
}

BlockTargets<Block> Instruction::get_targets() { return get_targets_generic<Block>(this); }

BlockTargets<const Block> Instruction::get_targets() const {
  return get_targets_generic<const Block>(this);
}

bool Instruction::is_volatile() const {
  switch (get_kind()) {
  case Kind::Ret:
  case Kind::Call:
  case Kind::Store:
  case Kind::Branch:
  case Kind::CondBranch:
    return true;

  default:
    return false;
  }
}
