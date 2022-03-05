#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename TInstruction, typename ValuePattern, bool MatchSpecificOp = false>
class UnaryPattern {
  TInstruction** bind_instruction;
  UnaryOp* bind_op;
  ValuePattern value_pattern;
  UnaryOp specific_op;

public:
  UnaryPattern(TInstruction** bind_instruction, UnaryOp* bind_op, ValuePattern value_pattern,
               UnaryOp specific_op = UnaryOp::Neg)
      : bind_instruction(bind_instruction), bind_op(bind_op), value_pattern(value_pattern),
        specific_op(specific_op) {}

  template <typename T> bool match(T* m_value) {
    const auto unary = ::cast<UnaryInstr>(m_value);
    if (!unary) {
      return false;
    }

    const auto op = unary->get_op();
    if (MatchSpecificOp && op != specific_op) {
      return false;
    }

    const bool matched = value_pattern.match(unary->get_val());
    if (!matched) {
      return false;
    }

    if (bind_op) {
      *bind_op = op;
    }

    if (bind_instruction) {
      *bind_instruction = unary;
    }

    return true;
  }
};

} // namespace detail

#define IMPLEMENT_SPECIFIC_UNARY_PATTERN(name, op)                                                 \
  template <typename TInstruction, typename ValuePattern>                                          \
  auto name(TInstruction*& instruction, ValuePattern value) {                                      \
    static_assert(std::is_same_v<UnaryInstr, std::remove_cv_t<TInstruction>>,                      \
                  "Expected UnaryInstr instruction in this pattern");                              \
    return unary_specific(instruction, op, value);                                                 \
  }                                                                                                \
                                                                                                   \
  template <typename ValuePattern> auto name(ValuePattern value) {                                 \
    return unary_specific(op, value);                                                              \
  }

template <typename TInstruction, typename ValuePattern>
auto unary(TInstruction*& instruction, UnaryOp& op, ValuePattern value) {
  static_assert(std::is_same_v<UnaryInstr, std::remove_cv_t<TInstruction>>,
                "Expected UnaryInstr instruction in this pattern");
  return detail::UnaryPattern<TInstruction, ValuePattern>(&instruction, &op, value);
}

template <typename ValuePattern> auto unary(UnaryOp& op, ValuePattern value) {
  return detail::UnaryPattern<const UnaryInstr, ValuePattern>(nullptr, &op, value);
}

template <typename TInstruction, typename ValuePattern>
auto unary(TInstruction*& instruction, ValuePattern value) {
  static_assert(std::is_same_v<UnaryInstr, std::remove_cv_t<TInstruction>>,
                "Expected UnaryInstr instruction in this pattern");
  return detail::UnaryPattern<TInstruction, ValuePattern>(&instruction, nullptr, value);
}

template <typename ValuePattern> auto unary(ValuePattern value) {
  return detail::UnaryPattern<const UnaryInstr, ValuePattern>(nullptr, nullptr, value);
}

template <typename TInstruction, typename ValuePattern>
auto unary_specific(TInstruction*& instruction, UnaryOp specific_op, ValuePattern value) {
  static_assert(std::is_same_v<UnaryInstr, std::remove_cv_t<TInstruction>>,
                "Expected UnaryInstr instruction in this pattern");
  return detail::UnaryPattern<TInstruction, ValuePattern, true>(&instruction, nullptr, value,
                                                                specific_op);
}

template <typename ValuePattern> auto unary_specific(UnaryOp specific_op, ValuePattern value) {
  return detail::UnaryPattern<const UnaryInstr, ValuePattern, true>(nullptr, nullptr, value,
                                                                    specific_op);
}

IMPLEMENT_SPECIFIC_UNARY_PATTERN(neg, UnaryOp::Neg)
IMPLEMENT_SPECIFIC_UNARY_PATTERN(not_, UnaryOp::Not)

#undef IMPLEMENT_SPECIFIC_UNARY_PATTERN

} // namespace flugzeug::pat