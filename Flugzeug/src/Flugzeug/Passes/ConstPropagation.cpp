#include "ConstPropagation.hpp"
#include "Utils/Evaluation.hpp"
#include "Utils/OptimizationResult.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>

using namespace flugzeug;

class Propagator : public InstructionVisitor {
  Type* type;

  static bool get_constant(const Value* value, uint64_t& result) {
    const auto c = cast<Constant>(value);
    if (c) {
      result = c->value_u();
    }
    return c;
  }

 public:
  explicit Propagator(Type* type) : type(type) {}

  OptimizationResult visit_unary_instr(Argument<UnaryInstr> unary) {
    uint64_t val;
    if (!get_constant(unary->value(), val)) {
      return OptimizationResult::unchanged();
    }

    return utils::evaluate_unary_instr_to_value(type, unary->op(), val);
  }

  OptimizationResult visit_binary_instr(Argument<BinaryInstr> binary) {
    uint64_t lhs, rhs;
    if (!get_constant(binary->lhs(), lhs) || !get_constant(binary->rhs(), rhs)) {
      return OptimizationResult::unchanged();
    }

    return utils::evaluate_binary_instr_to_value(type, lhs, binary->op(), rhs);
  }

  OptimizationResult visit_int_compare(Argument<IntCompare> int_compare) {
    uint64_t lhs, rhs;
    if (!get_constant(int_compare->lhs(), lhs) || !get_constant(int_compare->rhs(), rhs)) {
      return OptimizationResult::unchanged();
    }

    return utils::evaluate_int_compare_to_value(int_compare->lhs()->type(), lhs,
                                                int_compare->predicate(), rhs);
  }

  OptimizationResult visit_cast(Argument<Cast> cast) {
    uint64_t val;
    if (!get_constant(cast->casted_value(), val)) {
      return OptimizationResult::unchanged();
    }

    return utils::evaluate_cast_to_value(val, cast->casted_value()->type(), type,
                                         cast->cast_kind());
  }

  OptimizationResult visit_cond_branch(Argument<CondBranch> cond_branch) {
    uint64_t cond;
    if (!get_constant(cond_branch->condition(), cond)) {
      return OptimizationResult::unchanged();
    }

    const auto actual_target = cond_branch->select_target(cond);
    const auto removed_target = cond_branch->select_target(!cond);

    const auto block = cond_branch->block();
    const auto branch = new Branch(cond_branch->context(), actual_target);

    cond_branch->replace_with_instruction_and_destroy(branch);

    const bool destroy_empty_phis = removed_target != block;
    block->on_removed_branch_to(removed_target, destroy_empty_phis);

    return OptimizationResult::changed();
  }

  OptimizationResult visit_select(Argument<Select> select) {
    uint64_t cond;
    if (get_constant(select->condition(), cond)) {
      return select->select_value(cond);
    } else {
      return OptimizationResult::unchanged();
    }
  }

  OptimizationResult visit_offset(Argument<Offset> offset) {
    uint64_t base;
    const auto index_constant = cast<Constant>(offset->index());
    if (!get_constant(offset->base(), base) || !index_constant) {
      return OptimizationResult::unchanged();
    }

    const auto pointer = base + uint64_t(index_constant->value_i());

    return type->constant(pointer);
  }

  OptimizationResult visit_stackalloc(Argument<StackAlloc> stackalloc) {
    return OptimizationResult::unchanged();
  }
  OptimizationResult visit_phi(Argument<Phi> phi) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_load(Argument<Load> load) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_store(Argument<Store> store) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_call(Argument<Call> call) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_branch(Argument<Branch> branch) {
    return OptimizationResult::unchanged();
  }
  OptimizationResult visit_ret(Argument<Ret> ret) { return OptimizationResult::unchanged(); }
};

bool opt::ConstPropagation::run(Function* function) {
  bool did_something = false;

  for (Instruction& instruction : advance_early(function->instructions())) {
    Propagator propagator(instruction.type());

    if (const auto result = visitor::visit_instruction(&instruction, propagator)) {
      if (const auto replacement = result.replacement()) {
        instruction.replace_instruction_or_uses_and_destroy(replacement);
      }
      did_something = true;
    }
  }

  return did_something;
}