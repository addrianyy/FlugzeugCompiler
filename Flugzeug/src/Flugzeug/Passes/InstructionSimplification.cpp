#include "InstructionSimplification.hpp"
#include "Utils/Commutative.hpp"
#include "Utils/OptimizationResult.hpp"
#include "Utils/Propagation.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/Passes/Utils/SimplifyPhi.hpp>

using namespace flugzeug;

#define PROPAGATE_RESULT(call_expression)                                                          \
  do {                                                                                             \
    if (const auto _result = (call_expression)) {                                                  \
      return _result;                                                                              \
    }                                                                                              \
  } while (0)

static bool is_pow2(uint64_t x) { return (x != 0) && !(x & (x - 1)); }

static uint64_t bin_log2(uint64_t x) {
  for (uint64_t i = 1; i < 64; ++i) {
    if ((x >> i) & 1) {
      return i;
    }
  }

  return 0;
}

static OptimizationResult make_undef_if_uses_undef(Instruction* instruction) {
  for (size_t i = 0; i < instruction->get_operand_count(); ++i) {
    if (instruction->get_operand(i)->is_undef()) {
      return instruction->get_type()->get_undef();
    }
  }

  return OptimizationResult::unchanged();
}

static OptimizationResult chain_commutative_expressions(BinaryInstr* binary) {
  // Evaluate chain of (a op (C1 op (C2 op C3))) to (a op C).

  const auto op = binary->get_op();
  if (!BinaryInstr::is_binary_op_commutative(op)) {
    return OptimizationResult::unchanged();
  }

  BinaryInstr* parent_binary;
  Constant* constant;
  if (!utils::get_commutative_operation_operands(binary, parent_binary, constant)) {
    return OptimizationResult::unchanged();
  }

  if (op != parent_binary->get_op()) {
    return OptimizationResult::unchanged();
  }

  Value* operand;
  Constant* constant_2;
  if (!utils::get_commutative_operation_operands(parent_binary, operand, constant_2)) {
    return OptimizationResult::unchanged();
  }

  const auto evaluated = utils::propagate_binary_instr(
    binary->get_type(), constant->get_constant_u(), op, constant_2->get_constant_u());

  const auto evaluated_constant = binary->get_type()->get_constant(evaluated);
  const auto new_instruction =
    new BinaryInstr(binary->get_context(), operand, op, evaluated_constant);

  binary->replace_instruction_and_destroy(new_instruction);
  parent_binary->destroy_if_unused();

  return OptimizationResult::changed();
}

static OptimizationResult simplify_cmp_select_cmp_sequence(IntCompare* cmp) {
  // optimize this:
  //  v2 = cmp slt i1 v0, v1
  //  v3 = select i1 v2, i64 23, 88
  //  v4 = cmp ne i1 v3, 88
  //  ret i1 v4
  //
  // into:
  //  v2 = cmp slt i1 v0, v1
  //  ret i1 v2

  // In sequence of `cmp`, `select`, `cmp` we are matching on last compare.

  const IntPredicate pred = cmp->get_pred();
  if (pred != IntPredicate::Equal && pred != IntPredicate::NotEqual) {
    return OptimizationResult::unchanged();
  }

  // Order doesn't matter because we care only about EQ na NE predicates.
  Select* select;
  Constant* compared_to;
  if (!utils::get_commutative_operation_operands(cmp, select, compared_to)) {
    return OptimizationResult::unchanged();
  }

  const auto select_true = cast<Constant>(select->get_true_val());
  const auto select_false = cast<Constant>(select->get_false_val());
  if (!select_true || !select_false || select_true == select_false) {
    return OptimizationResult::unchanged();
  }

  bool inverted = false;

  // Get the corelation betwen first `cmp` and second `cmp`.
  // There can be 3 cases:
  // 1. second `cmp` result ==  first `cmp` result. (inverted = false)
  // 2. second `cmp` result == !first `cmp` result. (inverted = true)
  // 3. `cmps` are not corelated (in this case we exit).
  if (compared_to == select_true) {
    inverted = false;
  } else if (compared_to == select_false) {
    inverted = true;
  } else {
    return OptimizationResult::unchanged();
  }

  if (pred == IntPredicate::NotEqual) {
    inverted = !inverted;
  }

  // We know that both `cmps` are corelated with each other.
  if (!inverted) {
    cmp->replace_uses_and_destroy(select->get_cond());

    select->destroy_if_unused();

    return OptimizationResult::changed();
  } else if (const auto parent_cmp = cast<IntCompare>(select->get_cond())) {
    const auto new_cmp =
      new IntCompare(parent_cmp->get_context(), parent_cmp->get_lhs(),
                     IntCompare::inverted_predicate(parent_cmp->get_pred()), parent_cmp->get_rhs());
    cmp->replace_instruction_and_destroy(new_cmp);

    select->destroy_if_unused();
    parent_cmp->destroy_if_unused();

    return OptimizationResult::changed();
  }

  return OptimizationResult::unchanged();
}

class Simplifier : public InstructionVisitor {
  Context* context;

public:
  explicit Simplifier(Instruction* instruction) : context(instruction->get_context()) {}

  OptimizationResult visit_unary_instr(Argument<UnaryInstr> unary) {
    PROPAGATE_RESULT(make_undef_if_uses_undef(unary));

    // 2 same unary operations cancel out.
    // --x == x
    // !!x == x
    if (const auto other_unary = cast<UnaryInstr>(unary->get_val())) {
      if (unary->get_op() == other_unary->get_op()) {
        unary->replace_uses_and_destroy(other_unary->get_val());
        other_unary->destroy_if_unused();

        return OptimizationResult::changed();
      }
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_binary_instr(Argument<BinaryInstr> binary) {
    PROPAGATE_RESULT(make_undef_if_uses_undef(binary));
    PROPAGATE_RESULT(chain_commutative_expressions(binary));

    const auto zero = binary->get_type()->get_zero();

    if (binary->is(BinaryOp::Sub) && binary->get_lhs() == binary->get_rhs()) {
      // sub X, X == 0
      return zero;
    } else if (binary->is(BinaryOp::Add) && binary->get_rhs()->is_zero()) {
      // add X, 0 == X
      return binary->get_lhs();
    } else if (binary->is(BinaryOp::Mul)) {
      if (const auto multiplier_v = cast<Constant>(binary->get_rhs())) {
        const auto multiplier = multiplier_v->get_constant_u();
        if (multiplier == 0) {
          // mul X, 0 == 0
          return zero;
        } else if (multiplier == 1) {
          // mul X, 1 == X
          return binary->get_lhs();
        } else if (is_pow2(multiplier)) {
          // mul X, Y (if Y is power of 2) == shl X, log2(Y)
          const auto shift_amount = binary->get_type()->get_constant(bin_log2(multiplier));
          return new BinaryInstr(binary->get_context(), binary->get_lhs(), BinaryOp::Shl,
                                 shift_amount);
        }
      }
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_int_compare(Argument<IntCompare> int_compare) {
    PROPAGATE_RESULT(make_undef_if_uses_undef(int_compare));
    PROPAGATE_RESULT(simplify_cmp_select_cmp_sequence(int_compare));

    const auto lhs = int_compare->get_lhs();
    const auto rhs = int_compare->get_rhs();
    const auto pred = int_compare->get_pred();

    // If both operands to int compare instruction are the same we can
    // calculate the result at compile time.
    if (lhs == rhs) {
      const auto result = utils::propagate_int_compare(lhs->get_type(), 1, pred, 1);

      return int_compare->get_context()->get_i1_ty()->get_constant(result);
    }

    // Canonicalize `cmp const, non-const` to `cmp non-const, const`.
    if (cast<Constant>(lhs) && !cast<Constant>(rhs)) {
      int_compare->set_lhs(rhs);
      int_compare->set_rhs(lhs);
      int_compare->set_pred(IntCompare::swapped_order_predicate(pred));

      return OptimizationResult::changed();
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_cast(Argument<Cast> cast_instr) {
    PROPAGATE_RESULT(make_undef_if_uses_undef(cast_instr));

    const auto kind = cast_instr->get_cast_kind();

    if (const auto parent_cast = cast<Cast>(cast_instr->get_val())) {
      const auto parent_kind = parent_cast->get_cast_kind();

      // Two casts of the same type can be optimized out to only one.
      // For example `zext` from i16 to i32 and `zext` from i32 to i64
      // can be converted to `zext` from i16 to i64.
      if (kind == parent_kind ||
          (kind == CastKind::SignExtend && parent_kind == CastKind::ZeroExtend)) {
        const auto new_cast = new Cast(cast_instr->get_context(), parent_cast->get_val(),
                                       parent_kind, cast_instr->get_type());

        cast_instr->replace_instruction_and_destroy(new_cast);
        parent_cast->destroy_if_unused();

        return OptimizationResult::changed();
      }

      // v1 = zext i16 v0 to i64
      // v2 = trunc i64 v1 to i32
      //   =>
      // v1 = zext i16 v0 to i32
      if (kind == CastKind::Truncate &&
          (parent_kind == CastKind::ZeroExtend || parent_kind == CastKind::SignExtend)) {
        const auto original = parent_cast->get_val();

        const auto from_size = original->get_type()->get_bit_size();
        const auto to_size = cast_instr->get_type()->get_bit_size();

        CastKind new_kind = CastKind::Bitcast;
        if (from_size == to_size) {
          return original;
        } else if (from_size > to_size) {
          new_kind = CastKind::Truncate;
        } else if (from_size < to_size) {
          new_kind = parent_kind;
        }

        const auto new_cast =
          new Cast(cast_instr->get_context(), original, new_kind, cast_instr->get_type());

        cast_instr->replace_instruction_and_destroy(new_cast);
        parent_cast->destroy_if_unused();

        return OptimizationResult::changed();
      }
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_cond_branch(Argument<CondBranch> cond_branch) {
    const auto true_target = cond_branch->get_true_target();
    const auto false_target = cond_branch->get_false_target();

    // Branch to whatever target we want when condition is undefined.
    if (cond_branch->get_cond()->is_undef()) {
      return new Branch(context, false_target);
    }

    // Change conditional branch to unconditional if both targets are the same.
    if (true_target == false_target) {
      return new Branch(context, false_target);
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_select(Argument<Select> select) {
    const auto true_val = select->get_true_val();
    const auto false_val = select->get_false_val();

    // Return whichever value we want when condition is undefined.
    if (select->get_cond()->is_undef()) {
      return false_val;
    }

    // Select only non-undefined value.
    if (true_val->is_undef()) {
      return false_val;
    }
    if (false_val->is_undef()) {
      return true_val;
    }

    // We can remove this select if both values are the same.
    if (true_val == false_val) {
      return true_val;
    }

    // Remove sequences like this (some optimizations can create them).
    // v3 = cmp eq i32 v0, v2
    // v4 = select i1 v3, i32 v2, v0
    //   => v0
    if (const auto cmp = cast<IntCompare>(select->get_cond())) {
      const auto lhs = cmp->get_lhs();
      const auto rhs = cmp->get_rhs();
      const auto pred = cmp->get_pred();

      if (pred == IntPredicate::Equal || pred == IntPredicate::NotEqual) {
        const bool equal = pred == IntPredicate::Equal;

        Value* replacement = nullptr;

        if (true_val == lhs && false_val == rhs) {
          replacement = equal ? rhs : lhs;
        } else if (true_val == rhs && false_val == lhs) {
          replacement = equal ? lhs : rhs;
        }

        if (replacement) {
          return replacement;
        }
      }
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_phi(Argument<Phi> phi) {
    if (utils::simplify_phi(phi, true)) {
      return OptimizationResult::changed();
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_load(Argument<Load> load) {
    PROPAGATE_RESULT(make_undef_if_uses_undef(load));

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_store(Argument<Store> store) {
    if (store->get_ptr()->is_undef() || store->get_val()->is_undef()) {
      store->destroy();

      return OptimizationResult::changed();
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_offset(Argument<Offset> offset) {
    PROPAGATE_RESULT(make_undef_if_uses_undef(offset));

    // Offset with 0 index always returns base value.
    if (offset->get_index()->is_zero()) {
      return offset->get_base();
    }

    // GEP sign extends index internally so source the index
    // from non-sign-extended value.
    if (const auto cast_instr = cast<Cast>(offset->get_index())) {
      if (cast_instr->is(CastKind::SignExtend)) {
        offset->set_index(cast_instr->get_val());
        cast_instr->destroy_if_unused();

        return OptimizationResult::changed();
      }
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_stackalloc(Argument<StackAlloc> stackalloc) {
    return OptimizationResult::unchanged();
  }
  OptimizationResult visit_call(Argument<Call> call) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_branch(Argument<Branch> branch) {
    return OptimizationResult::unchanged();
  }
  OptimizationResult visit_ret(Argument<Ret> ret) { return OptimizationResult::unchanged(); }
};

bool InstructionSimplification::run(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    for (Instruction& instruction : dont_invalidate_current(block)) {
      Simplifier simplifier(&instruction);

      // Simplify current instruction. If we have inserted new instruction than try to simplify it
      // again.
      Instruction* current_instruction = &instruction;
      while (current_instruction) {
        if (const auto result = visitor::visit_instruction(current_instruction, simplifier)) {
          current_instruction = nullptr;

          if (const auto replacement = result.get_replacement()) {
            if (const auto new_instruction = cast<Instruction>(replacement)) {
              if (!new_instruction->get_block()) {
                // Try to simplify newly added instruction in the next iteration.
                current_instruction = new_instruction;
              }
            }

            instruction.replace_instruction_or_uses_and_destroy(replacement);
          }

          did_something = true;
        } else {
          break;
        }
      }
    }
  }

  return did_something;
}