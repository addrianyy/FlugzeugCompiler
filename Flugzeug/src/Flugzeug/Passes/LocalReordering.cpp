#include "LocalReordering.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/IR/Patterns.hpp>

using namespace flugzeug;

static std::optional<BinaryOp> corresponding_divmod(BinaryOp op) {
  // clang-format off
  switch (op) {
  case BinaryOp::ModU: return BinaryOp::DivU;
  case BinaryOp::DivU: return BinaryOp::ModU;
  case BinaryOp::ModS: return BinaryOp::DivS;
  case BinaryOp::DivS: return BinaryOp::ModS;
  default:             return std::nullopt;
  }
  // clang-format on
}

class Reorderer : InstructionVisitor {
 public:
  bool did_something = false;

  Instruction* visit_binary_instr(Argument<BinaryInstr> binary) {
    const auto corresponding_op = corresponding_divmod(binary->get_op());
    const auto previous = binary->previous();
    if (!corresponding_op || !previous) {
      return nullptr;
    }

    for (Instruction& instruction :
         instruction_range(binary->get_block()->get_first_instruction(), previous)) {
      if (match_pattern(&instruction,
                        pat::binary_specific(pat::exact(binary->get_lhs()), *corresponding_op,
                                             pat::exact(binary->get_rhs())))) {
        // Move this instruction just after corresponding `div`/`mod`.
        binary->move_after(&instruction);

        did_something = true;

        return nullptr;
      }
    }

    return nullptr;
  }

  Instruction* visit_load_store(Value* ptr) { return cast<Offset>(ptr); }
  Instruction* visit_select_cond_branch(Value* cond) { return cast<IntCompare>(cond); }

  Instruction* visit_load(Argument<Load> load) { return visit_load_store(load->get_ptr()); }
  Instruction* visit_store(Argument<Store> store) { return visit_load_store(store->get_ptr()); }

  Instruction* visit_cond_branch(Argument<CondBranch> cond_branch) {
    return visit_select_cond_branch(cond_branch->get_cond());
  }
  Instruction* visit_select(Argument<Select> select) {
    return visit_select_cond_branch(select->get_cond());
  }

  Instruction* visit_branch(Argument<Branch> branch) { return nullptr; }
  Instruction* visit_int_compare(Argument<IntCompare> int_compare) { return nullptr; }
  Instruction* visit_offset(Argument<Offset> offset) { return nullptr; }
  Instruction* visit_unary_instr(Argument<UnaryInstr> unary) { return nullptr; }
  Instruction* visit_call(Argument<Call> call) { return nullptr; }
  Instruction* visit_stackalloc(Argument<StackAlloc> stackalloc) { return nullptr; }
  Instruction* visit_ret(Argument<Ret> ret) { return nullptr; }
  Instruction* visit_cast(Argument<Cast> cast) { return nullptr; }
  Instruction* visit_phi(Argument<Phi> phi) { return nullptr; }
};

static bool can_move_earlier_down(Instruction* earlier_instruction,
                                  Instruction* later_instruction) {
  for (Instruction& instruction :
       instruction_range(earlier_instruction->next(), later_instruction)) {
    if (instruction.uses_value(earlier_instruction)) {
      return false;
    }
  }

  return true;
}

bool opt::LocalReordering::run(Function* function) {
  Reorderer reorderer;
  bool did_something = false;

  for (Instruction& instruction : advance_early(function->instructions())) {
    const auto later_instruction = &instruction;
    const auto earlier_instruction = visitor::visit_instruction(later_instruction, reorderer);
    if (!earlier_instruction) {
      continue;
    }

    // We want to move earlier instruction just before later instruction.

    // Local reorder can only reorder within one block.
    if (earlier_instruction->get_block() != later_instruction->get_block()) {
      continue;
    }

    // Check if other instruction is actually just above us. In this case
    // we have nothing to do.
    if (earlier_instruction->next() == later_instruction) {
      continue;
    }

    if (can_move_earlier_down(earlier_instruction, later_instruction)) {
      earlier_instruction->move_before(later_instruction);

      did_something = true;
    }
  }

  return did_something | reorderer.did_something;
}