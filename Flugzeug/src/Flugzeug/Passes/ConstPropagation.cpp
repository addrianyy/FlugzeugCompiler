#include "ConstPropagation.hpp"
#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>

using namespace flugzeug;

class Propagator : public InstructionVisitor {
public:
  Type* type;
  Block* removed_branch_target = nullptr;

  static bool get_constant(const Value* value, uint64_t& result) {
    const auto c = cast<Constant>(value);
    if (c) {
      result = c->get_constant_u();
    }
    return c;
  }

  template <typename T> static T propagate_unary(UnaryOp op, uint64_t value) {
    switch (op) {
    case UnaryOp::Neg:
      return T(-std::make_signed_t<T>(value));
    case UnaryOp::Not:
      return ~T(value);
    default:
      unreachable();
    }
  }

  template <typename T> static T propagate_binary(uint64_t lhs, BinaryOp op, uint64_t rhs) {
    static_assert(std::is_unsigned_v<T>);

    using Signed = std::make_signed_t<T>;
    using Unsigned = T;

    const auto sa = Signed(lhs);
    const auto sb = Signed(rhs);
    const auto ua = Unsigned(lhs);
    const auto ub = Unsigned(rhs);

    switch (op) {
    case BinaryOp::Add:
      return ua + ub;
    case BinaryOp::Sub:
      return ua - ub;
    case BinaryOp::Mul:
      return ua * ub;
    case BinaryOp::ModU:
      return ua % ub;
    case BinaryOp::DivU:
      return ua / ub;
    case BinaryOp::ModS:
      return sa % sb;
    case BinaryOp::DivS:
      return sa / sb;
    case BinaryOp::Shr:
      return ua >> ub;
    case BinaryOp::Shl:
      return ua << ub;
    case BinaryOp::Sar:
      return sa >> ub;
    case BinaryOp::And:
      return ua & ub;
    case BinaryOp::Or:
      return ua | ub;
    case BinaryOp::Xor:
      return ua ^ ub;
    default:
      unreachable();
    }
  }

  template <typename T>
  static bool propagate_int_compare(uint64_t lhs, IntPredicate pred, uint64_t rhs) {
    static_assert(std::is_unsigned_v<T>);

    using Signed = std::make_signed_t<T>;
    using Unsigned = T;

    const auto sa = Signed(lhs);
    const auto sb = Signed(rhs);
    const auto ua = Unsigned(lhs);
    const auto ub = Unsigned(rhs);

    switch (pred) {
    case IntPredicate::Equal:
      return ua == ub;
    case IntPredicate::NotEqual:
      return ua != ub;
    case IntPredicate::GtU:
      return ua > ub;
    case IntPredicate::GteU:
      return ua >= ub;
    case IntPredicate::GtS:
      return sa > sb;
    case IntPredicate::GteS:
      return sa >= sb;
    case IntPredicate::LtU:
      return ua < ub;
    case IntPredicate::LteU:
      return ua <= ub;
    case IntPredicate::LtS:
      return sa < sb;
    case IntPredicate::LteS:
      return sa <= sb;
    default:
      unreachable();
    }
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

  explicit Propagator(Type* type) : type(type) {}

  Value* visit_unary_instr(Argument<UnaryInstr> unary) {
    uint64_t val;
    if (!get_constant(unary->get_val(), val)) {
      return nullptr;
    }

    const auto op = unary->get_op();

    const uint64_t propagated = [&]() -> uint64_t {
      switch (type->get_kind()) {
      case Type::Kind::I8:
        return uint64_t(propagate_unary<uint8_t>(op, val));
      case Type::Kind::I16:
        return uint64_t(propagate_unary<uint16_t>(op, val));
      case Type::Kind::I32:
        return uint64_t(propagate_unary<uint32_t>(op, val));
      case Type::Kind::I64:
        return uint64_t(propagate_unary<uint64_t>(op, val));
      default:
        unreachable();
      }
    }();

    return type->get_constant(propagated);
  }

  Value* visit_binary_instr(Argument<BinaryInstr> binary) {
    uint64_t lhs, rhs;
    if (!get_constant(binary->get_lhs(), lhs) || !get_constant(binary->get_rhs(), rhs)) {
      return nullptr;
    }

    const auto op = binary->get_op();

    const uint64_t propagated = [&]() -> uint64_t {
      switch (type->get_kind()) {
      case Type::Kind::I8:
        return uint64_t(propagate_binary<uint8_t>(lhs, op, rhs));
      case Type::Kind::I16:
        return uint64_t(propagate_binary<uint16_t>(lhs, op, rhs));
      case Type::Kind::I32:
        return uint64_t(propagate_binary<uint32_t>(lhs, op, rhs));
      case Type::Kind::I64:
        return uint64_t(propagate_binary<uint64_t>(lhs, op, rhs));
      default:
        unreachable();
      }
    }();

    return type->get_constant(propagated);
  }

  Value* visit_int_compare(Argument<IntCompare> int_compare) {
    uint64_t lhs, rhs;
    if (!get_constant(int_compare->get_lhs(), lhs) || !get_constant(int_compare->get_rhs(), rhs)) {
      return nullptr;
    }

    const auto pred = int_compare->get_pred();

    const bool propagated = [&]() -> bool {
      switch (type->get_kind()) {
      case Type::Kind::I8:
        return propagate_int_compare<uint8_t>(lhs, pred, rhs);
      case Type::Kind::I16:
        return propagate_int_compare<uint16_t>(lhs, pred, rhs);
      case Type::Kind::I32:
        return propagate_int_compare<uint32_t>(lhs, pred, rhs);
      case Type::Kind::I64:
        return propagate_int_compare<uint64_t>(lhs, pred, rhs);
      default:
        unreachable();
      }
    }();

    return type->get_constant(propagated);
  }

  Value* visit_cast(Argument<Cast> cast) {
    uint64_t val;
    if (!get_constant(cast->get_val(), val)) {
      return nullptr;
    }

    const uint64_t propagated =
      propagate_cast(val, cast->get_val()->get_type(), type, cast->get_cast_kind());

    return type->get_constant(propagated);
  }

  Value* visit_cond_branch(Argument<CondBranch> cond_branch) {
    uint64_t cond;
    if (!get_constant(cond_branch->get_cond(), cond)) {
      return nullptr;
    }

    // We may have modified CFG so we need to update Phis.
    removed_branch_target = cond_branch->get_target(!cond);

    return new Branch(cond_branch->get_context(), cond_branch->get_target(cond));
  }

  Value* visit_select(Argument<Select> select) {
    uint64_t cond;
    return get_constant(select->get_cond(), cond) ? select->get_val(cond) : nullptr;
  }

  Value* visit_phi(Argument<Phi> phi) { return nullptr; }
  Value* visit_load(Argument<Load> load) { return nullptr; }
  Value* visit_store(Argument<Store> store) { return nullptr; }
  Value* visit_call(Argument<Call> call) { return nullptr; }
  Value* visit_branch(Argument<Branch> branch) { return nullptr; }
  Value* visit_ret(Argument<Ret> ret) { return nullptr; }
  Value* visit_offset(Argument<Offset> offset) { return nullptr; }
};

Value* ConstPropagation::constant_propagate(Instruction* instruction,
                                            Block*& removed_branch_target) {
  Propagator propagator(instruction->get_type());

  const auto replacement = visit_instruction(instruction, propagator);

  removed_branch_target = propagator.removed_branch_target;

  return replacement;
}

bool ConstPropagation::run(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    for (Instruction& instruction : dont_invalidate_current(block)) {
      Block* removed_branch_target = nullptr;

      if (const auto propagated = constant_propagate(&instruction, removed_branch_target)) {
        instruction.replace_instruction_or_uses_and_destroy(propagated);

        // Update Phis.
        if (removed_branch_target) {
          const bool destroy_empty_phis = removed_branch_target != &block;
          block.on_removed_branch_to(removed_branch_target, destroy_empty_phis);
        }

        did_something = true;
      }
    }
  }

  return did_something;
}