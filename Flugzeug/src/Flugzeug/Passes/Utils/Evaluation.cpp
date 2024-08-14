#include "Evaluation.hpp"

using namespace flugzeug;

template <typename T>
static T evaluate_unary_generic(UnaryOp op, uint64_t value) {
  switch (op) {
    case UnaryOp::Neg:
      return T(-std::make_signed_t<T>(value));
    case UnaryOp::Not:
      return ~T(value);
    default:
      unreachable();
  }
}

template <typename T>
static T evaluate_binary_generic(uint64_t lhs, BinaryOp op, uint64_t rhs) {
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
static bool evaluate_int_compare_generic(uint64_t lhs, IntPredicate pred, uint64_t rhs) {
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

uint64_t utils::evaluate_unary_instr(Type* type, UnaryOp op, uint64_t value) {
  switch (type->get_kind()) {
    case Type::Kind::I8:
      return uint64_t(evaluate_unary_generic<uint8_t>(op, value));
    case Type::Kind::I16:
      return uint64_t(evaluate_unary_generic<uint16_t>(op, value));
    case Type::Kind::I32:
      return uint64_t(evaluate_unary_generic<uint32_t>(op, value));
    case Type::Kind::I64:
      return uint64_t(evaluate_unary_generic<uint64_t>(op, value));
    default:
      unreachable();
  }
}

uint64_t utils::evaluate_binary_instr(Type* type, uint64_t lhs, BinaryOp op, uint64_t rhs) {
  switch (type->get_kind()) {
    case Type::Kind::I8:
      return uint64_t(evaluate_binary_generic<uint8_t>(lhs, op, rhs));
    case Type::Kind::I16:
      return uint64_t(evaluate_binary_generic<uint16_t>(lhs, op, rhs));
    case Type::Kind::I32:
      return uint64_t(evaluate_binary_generic<uint32_t>(lhs, op, rhs));
    case Type::Kind::I64:
      return uint64_t(evaluate_binary_generic<uint64_t>(lhs, op, rhs));
    default:
      unreachable();
  }
}

bool utils::evaluate_int_compare(Type* type, uint64_t lhs, IntPredicate predicate, uint64_t rhs) {
  switch (type->get_kind()) {
    case Type::Kind::I8:
      return evaluate_int_compare_generic<uint8_t>(lhs, predicate, rhs);
    case Type::Kind::I16:
      return evaluate_int_compare_generic<uint16_t>(lhs, predicate, rhs);
    case Type::Kind::I32:
      return evaluate_int_compare_generic<uint32_t>(lhs, predicate, rhs);
    case Type::Kind::I64:
    case Type::Kind::Pointer:
      return evaluate_int_compare_generic<uint64_t>(lhs, predicate, rhs);
    default:
      unreachable();
  }
}

uint64_t utils::evaluate_cast(uint64_t from, Type* from_type, Type* to_type, CastKind cast_kind) {
  const uint64_t from_mask = from_type->bit_mask();
  const uint64_t to_mask = to_type->bit_mask();

  const bool sign_bit = (from & (1ull << (from_type->bit_size() - 1))) != 0;

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