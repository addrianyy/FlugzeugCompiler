#include "Instruction.hpp"
#include "Block.hpp"
#include "Instructions.hpp"

using namespace flugzeug;

void Instruction::print(IRPrinter& printer) const {
  auto p = printer.create_line_printer();
  if (!get_type().is_void()) {
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
    replace_uses(get_context()->get_undef(get_type()));
  }
  IntrusiveNode::destroy();
}

BlockTargets<Block> Instruction::get_targets() {
  auto result = BlockTargets<Block>();
  if (const auto bcond = cast<CondBranch>(this)) {
    result.insert(bcond->get_true_target());
    result.insert(bcond->get_false_target());
  } else if (const auto branch = cast<Branch>(this)) {
    result.insert(branch->get_target());
  }

  return result;
}

BlockTargets<const Block> Instruction::get_targets() const {
  auto result = BlockTargets<const Block>();
  if (const auto bcond = cast<CondBranch>(this)) {
    result.insert(bcond->get_true_target());
    result.insert(bcond->get_false_target());
  } else if (const auto branch = cast<Branch>(this)) {
    result.insert(branch->get_target());
  }

  return result;
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
