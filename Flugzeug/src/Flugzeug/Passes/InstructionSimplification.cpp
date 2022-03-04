#include "InstructionSimplification.hpp"
#include "Utils/Commutative.hpp"
#include "Utils/Evaluation.hpp"
#include "Utils/OptimizationResult.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/Passes/Utils/SimplifyPhi.hpp>

#include <Flugzeug/IR/Patterns.hpp>

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
  for (const auto& operand : instruction->operands()) {
    if (operand.is_undef()) {
      return instruction->get_type()->get_undef();
    }
  }

  return OptimizationResult::unchanged();
}

static OptimizationResult chain_commutative_expressions(BinaryInstr* binary) {
  // Optimize chain of (C1 op (X op C2)) to (a op C).

  const auto op = binary->get_op();

  uint64_t c1, c2;
  Value* operand;
  BinaryInstr* parent_binary;

  if (!match_pattern(
        binary, pat::binary_commutative(pat::constant_u(c1),
                                        pat::binary_specific(parent_binary, pat::value(operand), op,
                                                             pat::constant_u(c2))))) {
    return OptimizationResult::unchanged();
  }

  const auto evaluated = utils::evaluate_binary_instr_to_value(binary->get_type(), c1, op, c2);

  binary->set_new_operands(operand, op, evaluated);
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

  Select* select;
  Constant* compared_to;
  IntPredicate pred;

  if (!match_pattern(cmp,
                     pat::compare_eq_or_ne(pat::value(select), pred, pat::constant(compared_to)))) {
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
    cmp->replace_uses_with_and_destroy(select->get_cond());

    select->destroy_if_unused();

    return OptimizationResult::changed();
  } else if (const auto parent_cmp = cast<IntCompare>(select->get_cond())) {
    const auto new_cmp =
      new IntCompare(cmp->get_context(), parent_cmp->get_lhs(),
                     IntCompare::inverted_predicate(parent_cmp->get_pred()), parent_cmp->get_rhs());
    cmp->replace_with_instruction_and_destroy(new_cmp);

    select->destroy_if_unused();
    parent_cmp->destroy_if_unused();

    return OptimizationResult::changed();
  }

  return OptimizationResult::unchanged();
}

static OptimizationResult bitcasts_to_offset(Cast* cast_instr) {
  // bitcast i32* ((bitcast i64 v0) + 16)
  //  =>
  // v0 offset by 4

  // bitcast i32* ((bitcast i64 v0) + (v1 * 4))
  //  =>
  // v0 offset by v1

  const auto context = cast_instr->get_context();
  const auto pointer_type = cast<PointerType>(cast_instr->get_type());

  // Make sure that it's a bitcast to a pointer type.
  if (!cast_instr->is(CastKind::Bitcast) || !pointer_type) {
    return OptimizationResult::unchanged();
  }

  const auto pointee_size = pointer_type->get_pointee()->get_byte_size();

  // Get add instruction which offsets the pointer. It must be operating on machine word sized
  // values.
  const auto add = cast<BinaryInstr>(cast_instr->get_val());
  if (!add || !add->is(BinaryOp::Add) || !add->get_type()->is_i64()) {
    return OptimizationResult::unchanged();
  }

  // Get possible offset amount and parent bitcast of the source pointer.
  Value* added_amount;
  Cast* parent_bitcast;
  if (!utils::get_commutative_operation_operands(add, added_amount, parent_bitcast)) {
    return OptimizationResult::unchanged();
  }

  // Get actual source pointer and verify its type.
  const auto parent_pointer = parent_bitcast->get_val();
  if (parent_pointer->get_type() != pointer_type) {
    return OptimizationResult::unchanged();
  }

  Value* offset_by = nullptr;
  if (const auto added_constant = cast<Constant>(added_amount)) {
    // (ptr + X) offsets ptr by (X / pointee_size) if X is divisible by it
    const auto v = added_constant->get_constant_u();
    if ((v % pointee_size) == 0) {
      offset_by = context->get_i64_ty()->get_constant(v / pointee_size);
    }
  } else if (const auto binary = cast<BinaryInstr>(added_amount)) {
    if (binary->is(BinaryOp::Mul)) {
      // (ptr + X * pointee_size) offsets ptr by X
      Value* other_value;
      Constant* multiplied_by;
      if (utils::get_commutative_operation_operands(binary, other_value, multiplied_by) &&
          multiplied_by->get_constant_u() == pointee_size) {
        offset_by = other_value;
      }
    } else if (binary->is(BinaryOp::Shl)) {
      // (ptr + X << log2(pointee_size)) offsets ptr by X
      const auto other_value = binary->get_lhs();
      const auto shifted_by = cast<Constant>(binary->get_rhs());
      if (shifted_by && (uint64_t(1) << shifted_by->get_constant_u()) == pointee_size) {
        offset_by = other_value;
      }
    }
  }

  if (!offset_by) {
    return OptimizationResult::unchanged();
  }

  cast_instr->replace_with_instruction_and_destroy(new Offset(context, parent_pointer, offset_by));

  if (const auto instruction = cast<Instruction>(added_amount)) {
    instruction->destroy_if_unused();
  }
  parent_bitcast->destroy_if_unused();

  return OptimizationResult::changed();
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
        unary->replace_uses_with_and_destroy(other_unary->get_val());
        other_unary->destroy_if_unused();

        return OptimizationResult::changed();
      }
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_binary_instr(Argument<BinaryInstr> binary) {
    PROPAGATE_RESULT(make_undef_if_uses_undef(binary));
    PROPAGATE_RESULT(chain_commutative_expressions(binary));

    const auto type = binary->get_type();
    const auto lhs = binary->get_lhs();
    const auto rhs = binary->get_rhs();

    // Canonicalize `op const, non-const` to `op non-const, const` if `op` is commutative.
    if (BinaryInstr::is_binary_op_commutative(binary->get_op())) {
      if (cast<Constant>(lhs) && !cast<Constant>(rhs)) {
        binary->set_lhs(rhs);
        binary->set_rhs(lhs);

        return OptimizationResult::changed();
      }
    }

    switch (binary->get_op()) {
    case BinaryOp::Add: {
      if (rhs->is_zero()) {
        // x + 0 == x
        return lhs;
      }

      break;
    }

    case BinaryOp::Sub: {
      if (lhs == rhs) {
        // x - x == 0
        return type->get_zero();
      }

      if (rhs->is_zero()) {
        // x - 0 == x
        return lhs;
      }

      if (lhs->is_zero()) {
        // 0 - x == -x
        return new UnaryInstr(context, UnaryOp::Neg, rhs);
      }

      if (const auto constant = cast<Constant>(rhs)) {
        // Canonicalize
        // x - c == x + (-c)

        // We do these casts to avoid compiler warning.
        const auto negated_constant =
          type->get_constant(uint64_t(-int64_t(constant->get_constant_u())));
        return new BinaryInstr(context, lhs, BinaryOp::Add, negated_constant);
      }

      break;
    }

    case BinaryOp::And: {
      if (rhs->is_zero()) {
        // x & 0 == 0
        return type->get_zero();
      }

      if (rhs->is_all_ones()) {
        // x & 111...111 == x
        return lhs;
      }

      if (lhs == rhs) {
        // x & x == x
        return lhs;
      }

      break;
    }

    case BinaryOp::Or: {
      if (rhs->is_zero()) {
        // x | 0 == x
        return lhs;
      }

      if (rhs->is_all_ones()) {
        // x | 111...111 == 111...111
        return rhs;
      }

      if (lhs == rhs) {
        // x | x == x
        return lhs;
      }

      break;
    }

    case BinaryOp::Xor: {
      if (rhs->is_zero()) {
        // x ^ 0 == x
        return lhs;
      }

      if (lhs == rhs) {
        // x ^ x == 0
        return type->get_zero();
      }

      if (rhs->is_all_ones()) {
        // a ^ 111...111 = ~a
        return new UnaryInstr(context, UnaryOp::Not, lhs);
      }

      break;
    }

    case BinaryOp::Mul: {
      if (rhs->is_zero()) {
        // x * 0 == 0
        return type->get_zero();
      }

      if (rhs->is_one()) {
        // x * 1 == x
        return lhs;
      }

      if (const auto multiplier_v = cast<Constant>(rhs)) {
        const auto multiplier = multiplier_v->get_constant_u();
        if (is_pow2(multiplier)) {
          // X * Y (if Y is power of 2) == X << log2(Y)
          const auto shift_amount = type->get_constant(bin_log2(multiplier));
          return new BinaryInstr(context, lhs, BinaryOp::Shl, shift_amount);
        }
      }

      break;
    }

    case BinaryOp::DivU:
    case BinaryOp::DivS: {
      if (lhs->is_zero()) {
        // 0 / x == 0
        return type->get_zero();
      }

      if (rhs->is_one()) {
        // x / 1 == x
        return lhs;
      }

      if (lhs == rhs) {
        // x / x == 1
        return type->get_one();
      }

      break;
    }

    case BinaryOp::ModU:
    case BinaryOp::ModS: {
      if (lhs->is_zero() || rhs->is_one() || lhs == rhs) {
        // 0 % x == 0
        // x % 1 == 0
        // x % x == 0
        return type->get_zero();
      }

      break;
    }

    case BinaryOp::Shr:
    case BinaryOp::Shl:
    case BinaryOp::Sar: {
      if (lhs->is_zero()) {
        // 0 <<>> x == 0
        return type->get_zero();
      }

      if (rhs->is_zero()) {
        // x <<>> 0 == x
        return lhs;
      }

      break;
    }

    default:
      unreachable();
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
      const auto result = utils::evaluate_int_compare(lhs->get_type(), 1, pred, 1);

      return context->get_i1_ty()->get_constant(result);
    }

    // Canonicalize `cmp const, non-const` to `cmp non-const, const`.
    if (cast<Constant>(lhs) && !cast<Constant>(rhs)) {
      int_compare->set_new_operands(rhs, IntCompare::swapped_order_predicate(pred), lhs);

      return OptimizationResult::changed();
    }

    const auto i1 = context->get_i1_ty();
    const auto false_value = i1->get_constant(0);
    const auto true_value = i1->get_constant(1);

    // Optimize some unsigned comparisons.
    if (rhs->is_zero()) {
      switch (pred) {
      case IntPredicate::LtU:
        // unsigned < 0 == false
        return false_value;

      case IntPredicate::LteU:
      case IntPredicate::GteU:
        // unsigned <= 0 == true
        // unsigned >= 0 == true
        return true_value;

      case IntPredicate::GtU:
        // unsigned > 0 == unsigned != 0
        return new IntCompare(context, lhs, IntPredicate::NotEqual,
                              lhs->get_type()->get_constant(0));

      default:
        break;
      }
    } else if (rhs->is_one() && pred == IntPredicate::LtU) {
      // unsigned < 1 == unsigned == 0
      return new IntCompare(context, lhs, IntPredicate::Equal, lhs->get_type()->get_constant(0));
    }

    // cmp (x + C1), C2  =>  cmp x, C2 - C1
    {
      BinaryInstr* add;
      Constant* cmp_constant;
      if (utils::get_commutative_operation_operands(int_compare, add, cmp_constant) &&
          add->is(BinaryOp::Add)) {
        Value* other;
        Constant* added_constant;
        if (utils::get_commutative_operation_operands(add, other, added_constant)) {
          const auto new_constant = cmp_constant->get_type()->get_constant(
            cmp_constant->get_constant_u() - added_constant->get_constant_u());

          int_compare->replace_operands(add, other);
          int_compare->replace_operands(cmp_constant, new_constant);

          add->destroy_if_unused();

          return OptimizationResult::changed();
        }
      }
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_cast(Argument<Cast> cast_instr) {
    PROPAGATE_RESULT(make_undef_if_uses_undef(cast_instr));
    PROPAGATE_RESULT(bitcasts_to_offset(cast_instr));

    const auto kind = cast_instr->get_cast_kind();

    if (const auto parent_cast = cast<Cast>(cast_instr->get_val())) {
      const auto parent_kind = parent_cast->get_cast_kind();

      // Two casts of the same type can be optimized out to only one.
      // For example `zext` from i16 to i32 and `zext` from i32 to i64
      // can be converted to `zext` from i16 to i64.
      if (kind == parent_kind ||
          (kind == CastKind::SignExtend && parent_kind == CastKind::ZeroExtend)) {
        const auto new_cast =
          new Cast(context, parent_cast->get_val(), parent_kind, cast_instr->get_type());

        cast_instr->replace_with_instruction_and_destroy(new_cast);
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

        const auto new_cast = new Cast(context, original, new_kind, cast_instr->get_type());

        cast_instr->replace_with_instruction_and_destroy(new_cast);
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

  for (Instruction& instruction : dont_invalidate_current(function->instructions())) {
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

  return did_something;
}