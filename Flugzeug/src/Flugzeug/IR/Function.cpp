#include "Function.hpp"
#include "ConsolePrinter.hpp"

using namespace flugzeug;

namespace flugzeug {
ValidationResults validate_function(const Function* function, ValidationBehaviour behaviour);
}

void Function::on_added_node(Block* block) {
  block->set_display_index(allocate_block_index());

  if (block->is_entry) {
    // We cannot check if size() == 0 because it's already updated before calling `on_added_node`.
    verify(!get_list().get_first(), "Entry block must be first one in the list.");
  }

  for (Instruction& instruction : *block) {
    if (!instruction.is_void()) {
      instruction.set_display_index(allocate_value_index());
    }
  }
}

void Function::on_removed_node(Block* block) {
  if (block == entry_block) {
    verify(get_list().is_empty(), "Entry block must be removed last");

    block->is_entry = false;
    entry_block = nullptr;
  }
}

Function::Function(Context* context, Type* return_type, std::string name,
                   const std::vector<Type*>& arguments)
    : Value(context, Value::Kind::Function, context->get_function_ty()), blocks(this),
      name(std::move(name)), return_type(return_type) {
  verify(return_type->is_arithmetic_or_pointer() || return_type->is_void(),
         "Function return type must be arithmetic or pointer or void");

  for (Type* type : arguments) {
    verify(type->is_arithmetic_or_pointer(),
           "Function parameter type must be arithmetic or pointer");

    auto parameter = new Parameter(context, type);
    parameter->set_display_index(allocate_value_index());

    parameters.push_back(parameter);
  }
}

Function::~Function() {
  verify(!entry_block, "There cannot be entry block before removing function.");
  verify(blocks.is_empty(), "Block list must be empty before removing function.");
}

ValidationResults Function::validate(ValidationBehaviour behaviour) const {
  if (!is_extern()) {
    return validate_function(this, behaviour);
  }

  return ValidationResults({});
}

void Function::print(bool newline) const {
  ConsolePrinter printer(ConsolePrinter::Variant::ColorfulIfSupported);

  print(printer);

  if (newline) {
    printer.newline();
  }
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

  auto printing_order =
    static_cast<const Block*>(entry_block)->get_reachable_blocks(TraversalType::BFS_WithStart);

  std::unordered_set<const Block*> reachable_blocks;
  reachable_blocks.reserve(printing_order.size());

  for (const Block* block : printing_order) {
    reachable_blocks.insert(block);
  }

  for (const Block& block : *this) {
    if (!reachable_blocks.contains(&block)) {
      printing_order.push_back(&block);
    }
  }

  for (const Block* block : printing_order) {
    block->print(printer);

    if (block != printing_order.back()) {
      printer.newline();
    }
  }

  printer.raw_write("}\n");
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
  auto block = new Block(get_context());
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

void Function::destroy() {
  for (Block& block : *this) {
    block.clear();
  }

  // Remove from last block so entry block will be removed last.
  while (!is_empty()) {
    get_last_block()->destroy();
  }

  // Remove all calls to this function.
  for (Instruction& instruction : dont_invalidate_current(users<Instruction>())) {
    instruction.destroy();
  }

  for (Parameter* param : parameters) {
    delete param;
  }

  delete this;
}