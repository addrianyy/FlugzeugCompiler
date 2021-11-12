#include "Block.hpp"
#include "Function.hpp"
#include "Instructions.hpp"

using namespace flugzeug;

void Block::on_added_node(Instruction* instruction) {
  if (get_function() && !instruction->is_void()) {
    instruction->set_display_index(get_function()->allocate_value_index());
  }
}

void Block::on_removed_node(Instruction* instruction) {}

Block::~Block() {
  verify(instructions.is_empty(), "Cannot remove non-empty block.");
  verify(!get_function(), "Cannot remove block that is attached to the function.");
}

std::string Block::format() const {
  return is_entry ? "entry" : ("block_" + std::to_string(get_display_index()));
}

void Block::print(IRPrinter& printer) const {
  {
    auto p = printer.create_line_printer();
    p.print(this, IRPrinter::Item::Colon);
  }

  for (const Instruction& instruction : instructions) {
    printer.tab();
    instruction.print(printer);
  }
}

void Block::remove_phi_incoming() {
  size_t index = 0;

  // We have to watch out for iterator invalidation here.
  while (index < get_uses().size()) {
    User* user = get_uses()[index].user;
    if (Phi* phi = cast<Phi>(user)) {
      phi->remove_incoming(this);
    } else {
      index++;
    }
  }
}

void Block::destroy() {
  // If this block is used by branch instructions we cannot remove it. Value destructor will
  // throw an error.

  remove_phi_incoming();
  IntrusiveNode::destroy();
}