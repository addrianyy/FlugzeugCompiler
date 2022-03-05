#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename TInstruction, typename LHSPattern, typename RHSPattern, bool MustBeCommutative,
          bool MatchSpecificOp = false>
class BinaryPattern {
  TInstruction** bind_instruction;
  LHSPattern lhs_pattern;
  BinaryOp* bind_op;
  RHSPattern rhs_pattern;
  BinaryOp specific_op;

public:
  BinaryPattern(TInstruction** bind_instruction, LHSPattern lhs, BinaryOp* bind_op, RHSPattern rhs,
                BinaryOp specific_op = BinaryOp::Add)
      : bind_instruction(bind_instruction), lhs_pattern(lhs), bind_op(bind_op), rhs_pattern(rhs),
        specific_op(specific_op) {}

  template <typename T> bool match(T* m_value) {
    const auto binary = ::cast<BinaryInstr>(m_value);
    if (!binary) {
      return false;
    }

    const auto op = binary->get_op();
    if (MatchSpecificOp && op != specific_op) {
      return false;
    }

    const auto is_commutative = BinaryInstr::is_binary_op_commutative(op);
    if (MustBeCommutative && !is_commutative) {
      return false;
    }

    const bool matched =
      (lhs_pattern.match(binary->get_lhs()) && rhs_pattern.match(binary->get_rhs())) ||
      (is_commutative && lhs_pattern.match(binary->get_rhs()) &&
       rhs_pattern.match(binary->get_lhs()));
    if (!matched) {
      return false;
    }

    if (bind_op) {
      *bind_op = op;
    }

    if (bind_instruction) {
      *bind_instruction = binary;
    }

    return true;
  }
};

} // namespace detail

#define IMPLEMENT_BINARY_PATTERN(name, commutative)                                                \
  template <typename TInstruction, typename LHSPattern, typename RHSPattern>                       \
  auto name(TInstruction*& instruction, LHSPattern lhs, BinaryOp& op, RHSPattern rhs) {            \
    static_assert(std::is_same_v<BinaryInstr, std::remove_cv_t<TInstruction>>,                     \
                  "Expected BinaryInstr instruction in this pattern");                             \
    return detail::BinaryPattern<TInstruction, LHSPattern, RHSPattern, commutative>(               \
      &instruction, lhs, &op, rhs);                                                                \
  }                                                                                                \
                                                                                                   \
  template <typename LHSPattern, typename RHSPattern>                                              \
  auto name(LHSPattern lhs, BinaryOp& op, RHSPattern rhs) {                                        \
    return detail::BinaryPattern<const BinaryInstr, LHSPattern, RHSPattern, commutative>(          \
      nullptr, lhs, &op, rhs);                                                                     \
  }                                                                                                \
                                                                                                   \
  template <typename TInstruction, typename LHSPattern, typename RHSPattern>                       \
  auto name(TInstruction*& instruction, LHSPattern lhs, RHSPattern rhs) {                          \
    static_assert(std::is_same_v<BinaryInstr, std::remove_cv_t<TInstruction>>,                     \
                  "Expected BinaryInstr instruction in this pattern");                             \
    return detail::BinaryPattern<TInstruction, LHSPattern, RHSPattern, commutative>(               \
      &instruction, lhs, nullptr, rhs);                                                            \
  }                                                                                                \
                                                                                                   \
  template <typename LHSPattern, typename RHSPattern> auto name(LHSPattern lhs, RHSPattern rhs) {  \
    return detail::BinaryPattern<const BinaryInstr, LHSPattern, RHSPattern, commutative>(          \
      nullptr, lhs, nullptr, rhs);                                                                 \
  }

#define IMPLEMENT_SPECIFIC_BINARY_PATTERN(name, op)                                                \
  template <typename TInstruction, typename LHSPattern, typename RHSPattern>                       \
  auto name(TInstruction*& instruction, LHSPattern lhs, RHSPattern rhs) {                          \
    return binary_specific(instruction, lhs, op, rhs);                                             \
  }                                                                                                \
                                                                                                   \
  template <typename LHSPattern, typename RHSPattern> auto name(LHSPattern lhs, RHSPattern rhs) {  \
    return binary_specific(lhs, op, rhs);                                                          \
  }

template <typename TInstruction, typename LHSPattern, typename RHSPattern>
auto binary_specific(TInstruction*& instruction, LHSPattern lhs, BinaryOp specific_op,
                     RHSPattern rhs) {
  static_assert(std::is_same_v<BinaryInstr, std::remove_cv_t<TInstruction>>,
                "Expected BinaryInstr instruction in this pattern");
  return detail::BinaryPattern<TInstruction, LHSPattern, RHSPattern, false, true>(
    &instruction, lhs, nullptr, rhs, specific_op);
}

template <typename LHSPattern, typename RHSPattern>
auto binary_specific(LHSPattern lhs, BinaryOp specific_op, RHSPattern rhs) {
  return detail::BinaryPattern<const BinaryInstr, LHSPattern, RHSPattern, false, true>(
    nullptr, lhs, nullptr, rhs, specific_op);
}

IMPLEMENT_BINARY_PATTERN(binary, false)
IMPLEMENT_BINARY_PATTERN(binary_commutative, true)

IMPLEMENT_SPECIFIC_BINARY_PATTERN(add, BinaryOp::Add)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(sub, BinaryOp::Sub)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(mul, BinaryOp::Mul)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(umod, BinaryOp::ModU)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(udiv, BinaryOp::DivU)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(smod, BinaryOp::ModS)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(sdiv, BinaryOp::DivS)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(shr, BinaryOp::Shr)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(shl, BinaryOp::Shl)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(sar, BinaryOp::Sar)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(and_, BinaryOp::And)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(or_, BinaryOp::Or)
IMPLEMENT_SPECIFIC_BINARY_PATTERN(xor_, BinaryOp::Xor)

#undef IMPLEMENT_BINARY_PATTERN
#undef IMPLEMENT_SPECIFIC_BINARY_PATTERN

} // namespace flugzeug::pat