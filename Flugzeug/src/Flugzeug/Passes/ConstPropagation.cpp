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
      result = c->get_constant_u();
    }
    return c;
  }

  static uint64_t propagate_cast(uint64_t from, Type* from_type, Type* to_type,
                                 CastKind cast_kind) {
    const uint64_t from_mask = from_type->get_bit_mask();
    const uint64_t to_mask = to_type->get_bit_mask();

    const bool sign_bit = (from & (1ull << (from_type->get_bit_size() - 1))) != 0;

    switch (cast_kind) {
    case CastKind::Bitcast:
    case CastKind::Truncate:
    case CastKind::ZeroExtend:
      return from & to_mask;
    case CastKind::SignExtend:
      return (from & to_mask) | (sign_bit ? (to_mask & ~from_mask) : 0);

    default:
      unreachable();
    }
  }

public:
  explicit Propagator(Type* type) : type(type) {}

  OptimizationResult visit_unary_instr(Argument<UnaryInstr> unary) {
    uint64_t val;
    if (!get_constant(unary->get_val(), val)) {
      return OptimizationResult::unchanged();
    }

    const auto propagated = utils::evaluate_unary_instr(type, unary->get_op(), val);

    return type->get_constant(propagated);
  }

  OptimizationResult visit_binary_instr(Argument<BinaryInstr> binary) {
    uint64_t lhs, rhs;
    if (!get_constant(binary->get_lhs(), lhs) || !get_constant(binary->get_rhs(), rhs)) {
      return OptimizationResult::unchanged();
    }

    const auto propagated = utils::evaluate_binary_instr(type, lhs, binary->get_op(), rhs);

    return type->get_constant(propagated);
  }

  OptimizationResult visit_int_compare(Argument<IntCompare> int_compare) {
    uint64_t lhs, rhs;
    if (!get_constant(int_compare->get_lhs(), lhs) || !get_constant(int_compare->get_rhs(), rhs)) {
      return OptimizationResult::unchanged();
    }

    const auto propagated = utils::evaluate_int_compare(type, lhs, int_compare->get_pred(), rhs);

    return type->get_constant(propagated);
  }

  OptimizationResult visit_cast(Argument<Cast> cast) {
    uint64_t val;
    if (!get_constant(cast->get_val(), val)) {
      return OptimizationResult::unchanged();
    }

    const uint64_t propagated =
      propagate_cast(val, cast->get_val()->get_type(), type, cast->get_cast_kind());

    return type->get_constant(propagated);
  }

  OptimizationResult visit_cond_branch(Argument<CondBranch> cond_branch) {
    uint64_t cond;
    if (!get_constant(cond_branch->get_cond(), cond)) {
      return OptimizationResult::unchanged();
    }

    const auto actual_target = cond_branch->get_target(cond);
    const auto removed_target = cond_branch->get_target(!cond);

    const auto block = cond_branch->get_block();
    const auto branch = new Branch(cond_branch->get_context(), actual_target);

    cond_branch->replace_instruction_and_destroy(branch);

    const bool destroy_empty_phis = removed_target != block;
    block->on_removed_branch_to(removed_target, destroy_empty_phis);

    return OptimizationResult::changed();
  }

  OptimizationResult visit_select(Argument<Select> select) {
    uint64_t cond;
    if (get_constant(select->get_cond(), cond)) {
      return select->get_val(cond);
    } else {
      return OptimizationResult::unchanged();
    }
  }

  OptimizationResult visit_offset(Argument<Offset> offset) {
    uint64_t base;
    const auto index_constant = cast<Constant>(offset->get_index());
    if (!get_constant(offset->get_base(), base) || !index_constant) {
      return OptimizationResult::unchanged();
    }

    const auto pointer = base + uint64_t(index_constant->get_constant_i());

    return type->get_constant(pointer);
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

bool ConstPropagation::run(Function* function) {
  bool did_something = false;

  for (Instruction& instruction : dont_invalidate_current(function->instructions())) {
    Propagator propagator(instruction.get_type());

    if (const auto result = visitor::visit_instruction(&instruction, propagator)) {
      if (const auto replacement = result.get_replacement()) {
        instruction.replace_instruction_or_uses_and_destroy(replacement);
      }
      did_something = true;
    }
  }

  return did_something;
}