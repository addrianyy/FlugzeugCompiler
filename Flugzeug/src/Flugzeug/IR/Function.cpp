#include "Function.hpp"
#include "ConsolePrinter.hpp"
#include "Internal/DebugPrinter.hpp"
#include "Module.hpp"

using namespace flugzeug;

namespace flugzeug {
ValidationResults validate_function(const Function* function, ValidationBehaviour behaviour);
}

void Function::on_added_node(Block* block) {
  block->set_display_index(allocate_block_index());

  if (block->is_entry) {
    // We cannot check if size() == 0 because it's already updated before calling `on_added_node`.
    verify(!intrusive_list().first(), "Entry block must be first one in the list.");
  }

  for (Instruction& instruction : *block) {
    if (!instruction.is_void()) {
      instruction.set_display_index(allocate_value_index());
    }
  }
}

void Function::on_removed_node(Block* block) {
  if (block->is_entry) {
    verify(intrusive_list().empty(), "Entry block must be removed last");
    block->is_entry = false;
  }
}

void Function::print_prototype(IRPrinter& printer, bool end_line) const {
  auto p = printer.create_line_printer();

  if (is_extern()) {
    p.print("extern");
  }

  p.print(return_type, IRPrinter::NonKeywordWord{name}, IRPrinter::Item::ParenOpen);

  for (auto param : parameters) {
    p.print(param->type(), param, IRPrinter::Item::Comma);
  }

  p.print(IRPrinter::Item::ParenClose);

  if (end_line) {
    p.print(IRPrinter::NonKeywordWord{is_extern() ? ";" : " {"});
  }
}

void Function::insert_block(Block* block) {
  if (blocks.empty()) {
    block->is_entry = true;
  }

  blocks.push_back(block);
}

Function::Function(Context* context,
                   Type* return_type,
                   std::string name,
                   const std::vector<Type*>& arguments)
    : Value(context, Value::Kind::Function, context->function_ty()),
      blocks(this),
      name(std::move(name)),
      return_type(return_type) {
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
  verify(blocks.empty(), "Block list must be empty before removing function.");
}

ValidationResults Function::validate(ValidationBehaviour behaviour) const {
  if (!is_extern()) {
    return validate_function(this, behaviour);
  }

  return ValidationResults({});
}

void Function::print(IRPrinter& printer, IRPrintingMethod method) const {
  print_prototype(printer, true);

  if (!is_extern()) {
    auto printing_order = get_entry_block()->get_reachable_blocks(TraversalType::BFS_WithStart);

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
      block->print(printer, method);

      if (block != printing_order.back()) {
        printer.newline();
      }
    }

    printer.raw_write("}\n");
  }
}

void Function::print(IRPrintingMethod method) const {
  ConsolePrinter printer(ConsolePrinter::Variant::ColorfulIfSupported);
  print(printer, method);
}

void Function::debug_print() const {
  DebugPrinter printer;
  print(printer);
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
  const auto block = new Block(context());
  insert_block(block);
  return block;
}

void Function::destroy() {
  for (Block& block : *this) {
    block.clear();
  }

  // Remove from last block so entry block will be removed last.
  while (!is_empty()) {
    get_last_block()->destroy();
  }

  for (Parameter* param : parameters) {
    delete param;
  }

  IntrusiveNode::destroy();
}

std::string Function::format() const {
  return name;
}
