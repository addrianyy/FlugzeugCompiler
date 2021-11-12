#include "Instruction.hpp"
#include "Block.hpp"

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