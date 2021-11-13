#include "Instruction.hpp"
#include "Block.hpp"
#include "Instructions.hpp"

using namespace flugzeug;

void Instruction::print(IRPrinter& printer) const {
  auto p = printer.create_line_printer();
  if (!is_void()) {
    p.print(this, IRPrinter::Item::Equals);
  }

  print_instruction_internal(p);
}

void Instruction::replace_instruction(Instruction* instruction) {
  instruction->insert_after(this);
  replace_uses(instruction);
  destroy();
}

void Instruction::destroy() {
  if (!is_void()) {
    replace_uses(get_type()->get_undef());
  }
  IntrusiveNode::destroy();
}

template <typename TBlock, typename TInstruction>
BlockTargets<TBlock> get_targets_generic(TInstruction* instruction) {
  BlockTargets<TBlock> result;
  if (const auto bcond = cast<CondBranch>(instruction)) {
    result.insert(bcond->get_true_target());
    result.insert(bcond->get_false_target());
  } else if (const auto branch = cast<Branch>(instruction)) {
    result.insert(branch->get_target());
  }

  return result;
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