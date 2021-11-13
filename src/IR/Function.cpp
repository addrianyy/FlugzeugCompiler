#include "Function.hpp"

using namespace flugzeug;

void Function::on_added_node(Block* block) {
  block->set_display_index(allocate_block_index());

  for (Instruction& instruction : *block) {
    if (!instruction.is_void()) {
      instruction.set_display_index(allocate_value_index());
    }
  }
}

void Function::on_removed_node(Block* block) {
  verify(block != entry_block, "Cannot remove entry block.");
}

Function::Function(Context* context, Type* return_type, std::string name,
                   const std::vector<Type*>& arguments)
    : blocks(this), context(context), name(std::move(name)), return_type(return_type) {
  context->increase_refcount();

  for (Type* type : arguments) {
    auto parameter = new Parameter(context, type);
    parameter->set_display_index(allocate_value_index());

    parameters.push_back(parameter);
  }
}

Function::~Function() {
  verify(blocks.is_empty(), "Block list must be empty before removing function.");

  context->decrease_refcount();
}

void Function::reassign_display_indices() {
  next_block_index = 0;
  next_value_index = 0;

  for (Parameter* parameter : parameters) {
    parameter->set_display_index(allocate_value_index());
  }

  for (Block& block : *this) {
    block.set_display_index(allocate_block_index());

    for (Instruction& instruction : block) {
      if (!instruction.is_void()) {
        instruction.set_display_index(allocate_value_index());
      }
    }
  }
}

Block* Function::create_block() {
  auto block = new Block(context);
  insert_block(block);
  return block;
}

void Function::insert_block(Block* block) {
  if (blocks.is_empty()) {
    entry_block = block;
    block->is_entry = true;
  }

  blocks.push_back(block);
}

void Function::set_entry_block(Block* block) {
  if (entry_block == block) {
    return;
  }

  if (entry_block) {
    entry_block->is_entry = false;
  }

  if (block) {
    verify(block->get_function() == this, "Entry block must be inserted.");
    block->is_entry = true;
  }

  entry_block = block;
}

void Function::print(IRPrinter& printer) const {
  {
    auto p = printer.create_line_printer();
    p.print(return_type, IRPrinter::NonKeywordWord{name}, IRPrinter::Item::ParenOpen);

    for (auto param : parameters) {
      p.print(param->get_type(), param, IRPrinter::Item::Comma);
    }

    p.print(IRPrinter::Item::ParenClose, IRPrinter::NonKeywordWord{" {"});
  }

  for (const Block& block : *this) {
    block.print(printer);

    if (&block != get_last_block()) {
      printer.newline();
    }
  }

  printer.raw_write("}\n");
}

void Function::destroy() {
  set_entry_block(nullptr);

  for (Block& block : *this) {
    while (!block.is_empty()) {
      block.get_first_instruction()->destroy();
    }
  }

  while (!is_empty()) {
    get_first_block()->destroy();
  }

  for (Parameter* param : parameters) {
    delete param;
  }

  delete this;
}