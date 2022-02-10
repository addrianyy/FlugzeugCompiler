#include "Instruction.hpp"
#include "Block.hpp"
#include "ConsolePrinter.hpp"
#include "DebugPrinter.hpp"
#include "Instructions.hpp"

using namespace flugzeug;

template <typename TBlock, typename TInstruction>
BlockTargets<TBlock> get_targets_generic(TInstruction* instruction) {
  BlockTargets<TBlock> result;
  if (const auto cond_branch = cast<CondBranch>(instruction)) {
    const auto true_target = cond_branch->get_true_target();
    const auto false_target = cond_branch->get_false_target();

    result.insert(true_target);

    if (true_target != false_target) {
      result.insert(false_target);
    }
  } else if (const auto branch = cast<Branch>(instruction)) {
    result.insert(branch->get_target());
  }

  return result;
}

void Instruction::print_possibly_inlined_value(
  const Value* value, IRPrinter::LinePrinter& printer,
  const std::unordered_set<const Value*>& inlined_values, bool parens) {
  if (const auto instruction = cast<Instruction>(value)) {
    if (inlined_values.contains(instruction)) {
      if (parens) {
        printer.print(IRPrinter::Item::ParenOpenExpr);
      }
      instruction->print_instruction_compact_internal(printer, inlined_values);
      if (parens) {
        printer.print(IRPrinter::Item::ParenCloseExpr);
      }
      return;
    }
  }

  printer.print(value);
}

size_t Instruction::get_order_in_block() const {
  get_block()->update_instruction_order();
  return order_in_block;
}

bool Instruction::print_compact(IRPrinter& printer,
                                const std::unordered_set<const Value*>& inlined_values) const {
  if (inlined_values.contains(this)) {
    return false;
  }

  auto p = printer.create_line_printer();
  if (!is_void()) {
    p.print(this, IRPrinter::Item::Equals);
  }

  print_instruction_compact_internal(p, inlined_values);

  return true;
}

void Instruction::print(IRPrinter& printer) const {
  auto p = printer.create_line_printer();
  if (!is_void()) {
    p.print(this, IRPrinter::Item::Equals);
  }

  print_instruction_internal(p);
}

void Instruction::print() const {
  ConsolePrinter printer(ConsolePrinter::Variant::ColorfulIfSupported);
  print(printer);
}

void Instruction::debug_print() const {
  DebugPrinter printer;
  print(printer);
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

bool Instruction::destroy_if_unused() {
  if (!is_used()) {
    destroy();
    return true;
  }

  return false;
}

void Instruction::replace_with_instruction_and_destroy(Instruction* instruction) {
  verify(instruction != this, "Cannot replace instruction with itself");

  instruction->insert_after(this);
  if (!is_void()) {
    replace_uses_with(instruction);
  }
  destroy();
}

void Instruction::replace_uses_with_constant_and_destroy(uint64_t constant) {
  replace_uses_with_constant(constant);
  destroy();
}

void Instruction::replace_uses_with_and_destroy(Value* new_value) {
  verify(new_value != this, "Cannot replace instruction with itself");

  replace_uses_with(new_value);
  destroy();
}

void Instruction::replace_instruction_or_uses_and_destroy(Value* new_value) {
  verify(new_value != this, "Cannot replace instruction with itself");

  const auto other = cast<Instruction>(new_value);
  if (other && !other->get_block()) {
    replace_with_instruction_and_destroy(other);
  } else {
    replace_uses_with_and_destroy(new_value);
  }
}

BlockTargets<Block> Instruction::targets() { return get_targets_generic<Block>(this); }

BlockTargets<const Block> Instruction::targets() const {
  return get_targets_generic<const Block>(this);
}

bool Instruction::is_before(const Instruction* other) const {
  verify(get_block() == other->get_block(),
         "Cannot call `is_before` on instructions in different blocks");
  return get_order_in_block() < other->get_order_in_block();
}

bool Instruction::is_after(const Instruction* other) const {
  verify(get_block() == other->get_block(),
         "Cannot call `is_after` on instructions in different blocks");
  return get_order_in_block() > other->get_order_in_block();
}

bool Instruction::is_dominated_by(const Block* block, const DominatorTree& dominator_tree) const {
  return get_block()->is_dominated_by(block, dominator_tree);
}

bool Instruction::dominates(const Instruction* other, const DominatorTree& dominator_tree) const {
  if (this == other) {
    return true;
  }

  const auto this_block = get_block();
  const auto other_block = other->get_block();

  if (this_block == other_block) {
    return is_before(other);
  } else {
    return this_block->dominates(other_block, dominator_tree);
  }
}

bool Instruction::is_dominated_by(const Instruction* other,
                                  const DominatorTree& dominator_tree) const {
  return other->dominates(this, dominator_tree);
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
bool Instruction::is_branching() const {
  return get_kind() == Kind::Branch || get_kind() == Kind::CondBranch;
}

bool Instruction::is_terminator() const { return is_branching() || get_kind() == Kind::Ret; }
