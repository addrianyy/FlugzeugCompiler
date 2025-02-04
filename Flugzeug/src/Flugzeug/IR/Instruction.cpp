#include "Instruction.hpp"
#include "Block.hpp"
#include "ConsoleIRPrinter.hpp"
#include "InstructionVisitor.hpp"
#include "Instructions.hpp"
#include "Internal/DebugIRPrinter.hpp"

using namespace flugzeug;

template <typename TBlock, typename TInstruction>
BlockTargets<TBlock> get_targets_generic(TInstruction* instruction) {
  BlockTargets<TBlock> result;
  if (const auto cond_branch = cast<CondBranch>(instruction)) {
    const auto true_target = cond_branch->true_target();
    const auto false_target = cond_branch->false_target();

    result.push_back(true_target);

    if (true_target != false_target) {
      result.push_back(false_target);
    }
  } else if (const auto branch = cast<Branch>(instruction)) {
    result.push_back(branch->target());
  }

  return result;
}

void Instruction::print_value_compact(const Value* value,
                                      IRPrinter::LinePrinter& printer,
                                      const std::unordered_set<const Value*>& inlined_values,
                                      bool parens) {
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

size_t Instruction::order_in_block() const {
  block()->update_instruction_order();
  return order_in_block_;
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
  ConsoleIRPrinter printer(ConsoleIRPrinter::Variant::ColorfulIfSupported);
  print(printer);
}

void Instruction::debug_print() const {
  DebugIRPrinter printer;
  print(printer);
}

Function* Instruction::function() {
  return block() ? block()->function() : nullptr;
}

const Function* Instruction::function() const {
  return block() ? block()->function() : nullptr;
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
  if (other && !other->block()) {
    replace_with_instruction_and_destroy(other);
  } else {
    replace_uses_with_and_destroy(new_value);
  }
}

BlockTargets<Block> Instruction::targets() {
  return get_targets_generic<Block>(this);
}

BlockTargets<const Block> Instruction::targets() const {
  return get_targets_generic<const Block>(this);
}

bool Instruction::is_before(const Instruction* other) const {
  verify(block() == other->block(), "Cannot call `is_before` on instructions in different blocks");
  return order_in_block() < other->order_in_block();
}

bool Instruction::is_after(const Instruction* other) const {
  verify(block() == other->block(), "Cannot call `is_after` on instructions in different blocks");
  return order_in_block() > other->order_in_block();
}

bool Instruction::is_dominated_by(const Block* block_, const DominatorTree& dominator_tree) const {
  return block()->is_dominated_by(block_, dominator_tree);
}

bool Instruction::dominates(const Instruction* other, const DominatorTree& dominator_tree) const {
  if (this == other) {
    return true;
  }

  const auto this_block = block();
  const auto other_block = other->block();

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
  switch (kind()) {
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
  return kind() == Kind::Branch || kind() == Kind::CondBranch;
}

bool Instruction::is_terminator() const {
  return is_branching() || kind() == Kind::Ret;
}
