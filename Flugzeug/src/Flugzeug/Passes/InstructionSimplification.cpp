#include "InstructionSimplification.hpp"
#include "Utils/Evaluation.hpp"
#include "Utils/OptimizationResult.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/Passes/Utils/SimplifyPhi.hpp>

#include <Flugzeug/IR/Patterns.hpp>

using namespace flugzeug;

#define PROPAGATE_RESULT(result_expression)                                                        \
  do {                                                                                             \
    if (const auto _result = (result_expression)) {                                                \
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

static OptimizationResult simplify_arithmetic(BinaryInstr* binary) {
  {
    Value* a;

    // a + (-a) => 0
    if (match_pattern(binary, pat::add(pat::neg(pat::value(a)), pat::exact_ref(a)))) {
      return binary->get_type()->get_zero();
    }
  }

  {
    Value* a;
    Value* b;
    Value* c;
    Value* common;

    // (a * b) + (a * c) => a * (b + c)
    if (match_pattern(binary,
                      pat::add(pat::mul(pat::value(a), pat::value(b)),
                               pat::mul(pat::either(common, pat::exact_ref(a), pat::exact_ref(b)),
                                        pat::value(c))))) {
      if (common == b) {
        std::swap(a, b);
      }

      return OptimizationResult::rewrite(binary,
                                         [&](Rewriter& r) { return r.mul(a, r.add(b, c)); });
    }
  }

  return OptimizationResult::unchanged();
}

static OptimizationResult simplify_bit_operations(BinaryInstr* binary) {
  {
    Value* x;
    Value* y;

    // ((x & y) | (x & ~y)) => x
    if (match_pattern(binary, pat::or_(pat::and_(pat::not_(pat::value(x)), pat::value(y)),
                                       pat::and_(pat::exact_ref(x), pat::exact_ref(y))))) {
      return x;
    }
  }

  {
    Value* x;

    // x & ~x => 0
    if (match_pattern(binary, pat::and_(pat::value(x), pat::not_(pat::exact_ref(x))))) {
      return binary->get_type()->get_zero();
    }
  }

  {
    Value* x;
    Value* y;
    Value* z;

    // (x ^ y) ^ y => x
    if (match_pattern(binary, pat::xor_(pat::xor_(pat::value(x), pat::value(y)),
                                        pat::either(z, pat::exact_ref(x), pat::exact_ref(y))))) {
      return z == x ? y : x;
    }
  }

  return OptimizationResult::unchanged();
}

static bool is_value_compared_to(Value* value, Value*& cmp_value, int64_t& cmp_constant) {
  if (value == cmp_value) {
    return true;
  }

  int64_t add_constant = 0;
  if (match_pattern(value, pat::add(pat::exact(cmp_value), pat::constant_i(add_constant)))) {
    cmp_constant = Constant::constrain_i(cmp_value->get_type(), cmp_constant + add_constant);
    cmp_value = value;
    return true;
  }

  return false;
}

static OptimizationResult simplify_selected_arithmetic_1(Value* cmp_value, int64_t cmp_constant,
                                                         Value* on_constant,
                                                         BinaryInstr* on_non_constant) {
  // (b != 0) ? (a - b) : a   =>   a - b

  const auto op = on_non_constant->get_op();
  const auto lhs = on_non_constant->get_lhs();
  const auto rhs = on_non_constant->get_rhs();

  if (lhs != on_constant && rhs != on_constant) {
    return OptimizationResult::unchanged();
  }

  if (lhs == on_constant) {
    if (!is_value_compared_to(rhs, cmp_value, cmp_constant)) {
      return OptimizationResult::unchanged();
    }
  } else if (rhs == on_constant) {
    if (!is_value_compared_to(lhs, cmp_value, cmp_constant)) {
      return OptimizationResult::unchanged();
    }
  }

  switch (op) {
  case BinaryOp::Add: {
    if (cmp_constant == 0) {
      return on_non_constant;
    }
    break;
  }
  case BinaryOp::Sub: {
    if (cmp_constant == 0 && rhs == cmp_value) {
      return on_non_constant;
    }
    break;
  }

  case BinaryOp::Mul: {
    if (cmp_constant == 1) {
      return on_non_constant;
    }
    break;
  }

  case BinaryOp::DivU:
  case BinaryOp::DivS: {
    if (cmp_constant == 1 && rhs == cmp_value) {
      return on_non_constant;
    }
    break;
  }

  case BinaryOp::Shr:
  case BinaryOp::Shl:
  case BinaryOp::Sar: {
    if (cmp_constant == 0 && rhs == cmp_value) {
      return on_non_constant;
    }
    break;
  }

  case BinaryOp::And: {
    if (cmp_constant == -1) {
      return on_non_constant;
    }
    break;
  }

  case BinaryOp::Or: {
  case BinaryOp::Xor:
    if (cmp_constant == 0) {
      return on_non_constant;
    }
    break;
  }

  default:
    break;
  }

  return OptimizationResult::unchanged();
}

static OptimizationResult simplify_selected_arithmetic_2(Value* cmp_value, int64_t cmp_constant,
                                                         Value* on_constant_value,
                                                         BinaryInstr* on_non_constant) {
  // (b != 0) ? (a * b) : 0   =>   a * b

  const auto on_constant_instruction = cast<Constant>(on_constant_value);
  if (!on_constant_instruction) {
    return OptimizationResult::unchanged();
  }
  const auto on_constant = on_constant_instruction->get_constant_i();

  if (!is_value_compared_to(on_non_constant->get_lhs(), cmp_value, cmp_constant) &&
      !is_value_compared_to(on_non_constant->get_rhs(), cmp_value, cmp_constant)) {
    return OptimizationResult::unchanged();
  }

  switch (on_non_constant->get_op()) {
  case BinaryOp::Mul:
  case BinaryOp::And: {
    if (cmp_constant == 0 && on_constant == 0) {
      return on_non_constant;
    }
    break;
  }

  case BinaryOp::Or: {
    if (cmp_constant == -1 && on_constant == -1) {
      return on_non_constant;
    }
    break;
  }

  default:
    break;
  }

  return OptimizationResult::unchanged();
}

static OptimizationResult simplify_selected_arithmetic(Select* select) {
  // (b != 0) ? (a - b) : a   =>   a - b
  // (b != 0) ? (a * b) : 0   =>   a * b

  Value* cmp_value;
  int64_t cmp_constant;
  IntPredicate predicate;
  if (!match_pattern(select->get_cond(), pat::compare_eq_or_ne(pat::constant_i(cmp_constant),
                                                               predicate, pat::value(cmp_value)))) {
    return OptimizationResult::unchanged();
  }

  const bool constant_equal = predicate == IntPredicate::Equal;

  const auto on_constant = select->get_val(constant_equal);

  const auto on_non_constant = cast<BinaryInstr>(select->get_val(!constant_equal));
  if (!on_non_constant) {
    return OptimizationResult::unchanged();
  }

  PROPAGATE_RESULT(
    simplify_selected_arithmetic_1(cmp_value, cmp_constant, on_constant, on_non_constant));
  PROPAGATE_RESULT(
    simplify_selected_arithmetic_2(cmp_value, cmp_constant, on_constant, on_non_constant));

  return OptimizationResult::unchanged();
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

  IntPredicate pred;
  Select* select;
  Constant* compared_to;

  Value* select_cond;
  Constant* select_true;
  Constant* select_false;

  {
    const auto select_pat = pat::select(select, pat::value(select_cond), pat::constant(select_true),
                                        pat::constant(select_false));

    if (!match_pattern(cmp, pat::compare_eq_or_ne(select_pat, pred, pat::constant(compared_to)))) {
      return OptimizationResult::unchanged();
    }
  }

  if (select_true == select_false) {
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
    cmp->replace_uses_with_and_destroy(select_cond);

    select->destroy_if_unused();

    return OptimizationResult::changed();
  } else if (const auto parent_cmp = cast<IntCompare>(select_cond)) {
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
  if (!pointer_type) {
    return OptimizationResult::unchanged();
  }
  const auto i64 = context->get_i64_ty();

  Value* source_pointer;
  Value* added_amount;

  Cast* parent_bitcast;
  BinaryInstr* add;

  {
    const auto source_pointer_pat = pat::typed(pointer_type, pat::value(source_pointer));
    const auto bitcast_source_to_i64 =
      pat::typed(i64, pat::bitcast(parent_bitcast, source_pointer_pat));
    const auto add_offset_to_source =
      pat::add(add, bitcast_source_to_i64, pat::value(added_amount));

    // bitcast ((bitcast source_pointer) + added_amount)
    if (!match_pattern(cast_instr, pat::bitcast(add_offset_to_source))) {
      return OptimizationResult::unchanged();
    }
  }

  const auto pointee_size = pointer_type->get_pointee()->get_byte_size();

  Value* offset_by = nullptr;
  if (const auto added_constant = cast<Constant>(added_amount)) {
    // (ptr + X) offsets ptr by (X / pointee_size) if X is divisible by it
    const auto v = added_constant->get_constant_u();
    if ((v % pointee_size) == 0) {
      offset_by = i64->get_constant(v / pointee_size);
    }
  } else if (const auto binary = cast<BinaryInstr>(added_amount)) {
    Value* other_value;
    uint64_t constant;
    if (match_pattern(binary, pat::mul(pat::value(other_value), pat::constant_u(constant)))) {
      // (ptr + X * pointee_size) offsets ptr by X
      if (constant == pointee_size) {
        offset_by = other_value;
      }
    } else if (match_pattern(binary,
                             pat::shl(pat::value(other_value), pat::constant_u(constant)))) {
      // (ptr + X << log2(pointee_size)) offsets ptr by X
      if ((uint64_t(1) << constant) == pointee_size) {
        offset_by = other_value;
      }
    }
  }

  if (!offset_by) {
    return OptimizationResult::unchanged();
  }

  cast_instr->replace_with_instruction_and_destroy(new Offset(context, source_pointer, offset_by));

  if (const auto instruction = cast<Instruction>(added_amount)) {
    instruction->destroy_if_unused();
  }
  parent_bitcast->destroy_if_unused();
  add->destroy_if_unused();

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
    PROPAGATE_RESULT(simplify_bit_operations(binary));
    PROPAGATE_RESULT(simplify_arithmetic(binary));

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

      if (rhs->is_all_ones()) {
        // a * -1 == -a
        return new UnaryInstr(context, UnaryOp::Neg, lhs);
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
      Value* add_unknown;
      Constant* add_const;
      Constant* compared_to;
      BinaryInstr* add;

      const auto added = pat::add(add, pat::constant(add_const), pat::value(add_unknown));

      if (match_pattern(int_compare, pat::compare_eq_or_ne(added, pat::constant(compared_to)))) {
        const auto new_constant = compared_to->get_type()->get_constant(
          compared_to->get_constant_u() - add_const->get_constant_u());

        int_compare->replace_operands(add, add_unknown);
        int_compare->replace_operands(compared_to, new_constant);

        add->destroy_if_unused();

        return OptimizationResult::changed();
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
          new Cast(context, parent_kind, parent_cast->get_val(), cast_instr->get_type());

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

        const auto new_cast = new Cast(context, new_kind, original, cast_instr->get_type());

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
      const auto block = cond_branch->get_block();
      const auto other = cond_branch->get_true_target();

      cond_branch->replace_with_instruction_and_destroy(new Branch(context, false_target));

      block->on_removed_branch_to(other, true);

      return OptimizationResult::changed();
    }

    // Change conditional branch to unconditional if both targets are the same.
    if (true_target == false_target) {
      return new Branch(context, false_target);
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_select(Argument<Select> select) {
    PROPAGATE_RESULT(simplify_selected_arithmetic(select));

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

    {
      Value* common;
      Value* on_true_value;
      Value* on_false_value;
      BinaryInstr* on_true;
      BinaryInstr* on_false;

      //   select (a + X), (a + Y)
      // =>
      //   a + (select X, Y)
      if (match_pattern(
            select,
            pat::select(
              pat::value(), pat::binary(on_true, pat::value(common), pat::value(on_true_value)),
              pat::binary(on_false, pat::exact_ref(common), pat::value(on_false_value))))) {
        if (on_true->get_op() == on_false->get_op()) {
          select->set_true_val(on_true_value);
          select->set_false_val(on_false_value);

          const auto new_binary = new BinaryInstr(context, common, on_true->get_op(), select);
          new_binary->insert_after(select);
          select->replace_uses_with_predicated(new_binary,
                                               [&](User* user) { return user != new_binary; });

          on_true->destroy_if_unused();
          on_false->destroy_if_unused();

          return OptimizationResult::changed();
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

bool opt::InstructionSimplification::run(Function* function) {
  bool did_something = false;

  for (Instruction& instruction : advance_early(function->instructions())) {
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