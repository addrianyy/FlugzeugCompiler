#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename ValuePattern, bool MatchSpecificOp = false> class UnaryPattern {
  UnaryInstr** bind_instruction;
  UnaryOp* bind_op;
  ValuePattern value_pattern;
  UnaryOp specific_op;

public:
  UnaryPattern(UnaryInstr** bind_instruction, UnaryOp* bind_op, ValuePattern value_pattern,
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
  template <typename ValuePattern> auto name(UnaryInstr*& instruction, ValuePattern value) {       \
    return unary_specific(instruction, op, value);                                                 \
  }                                                                                                \
                                                                                                   \
  template <typename ValuePattern> auto name(ValuePattern value) {                                 \
    return unary_specific(op, value);                                                              \
  }

template <typename ValuePattern>
auto unary(UnaryInstr*& instruction, UnaryOp& op, ValuePattern value) {
  return detail::UnaryPattern<ValuePattern>(&instruction, &op, value);
}

template <typename ValuePattern> auto unary(UnaryOp& op, ValuePattern value) {
  return detail::UnaryPattern<ValuePattern>(nullptr, &op, value);
}

template <typename ValuePattern> auto unary(UnaryInstr*& instruction, ValuePattern value) {
  return detail::UnaryPattern<ValuePattern>(&instruction, nullptr, value);
}

template <typename ValuePattern> auto unary(ValuePattern value) {
  return detail::UnaryPattern<ValuePattern>(nullptr, nullptr, value);
}

template <typename ValuePattern>
auto unary_specific(UnaryInstr*& instruction, UnaryOp specific_op, ValuePattern value) {
  return detail::UnaryPattern<ValuePattern, true>(&instruction, nullptr, value, specific_op);
}

template <typename ValuePattern> auto unary_specific(UnaryOp specific_op, ValuePattern value) {
  return detail::UnaryPattern<ValuePattern, true>(nullptr, nullptr, value, specific_op);
}

IMPLEMENT_SPECIFIC_UNARY_PATTERN(neg, UnaryOp::Neg)
IMPLEMENT_SPECIFIC_UNARY_PATTERN(not_, UnaryOp::Not)

#undef IMPLEMENT_SPECIFIC_UNARY_PATTERN

} // namespace flugzeug::pat