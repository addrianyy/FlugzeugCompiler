#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename ValuePattern, bool MatchSpecificCast = false> class CastPattern {
  Cast** bind_instruction;
  CastKind* bind_kind;
  ValuePattern value;
  CastKind specific_kind;

public:
  CastPattern(Cast** bind_instruction, CastKind* bind_kind, ValuePattern value,
              CastKind specific_kind = CastKind::Bitcast)
      : bind_instruction(bind_instruction), bind_kind(bind_kind), value(value),
        specific_kind(specific_kind) {}

  template <typename T> bool match(T* m_value) {
    const auto cast_instr = ::cast<Cast>(m_value);
    if (!cast_instr) {
      return false;
    }

    const auto kind = cast_instr->get_cast_kind();
    if (MatchSpecificCast && kind != specific_kind) {
      return false;
    }

    const bool matched = value.match(cast_instr->get_val());
    if (!matched) {
      return false;
    }

    if (bind_kind) {
      *bind_kind = kind;
    }

    if (bind_instruction) {
      *bind_instruction = cast_instr;
    }

    return true;
  }
};

} // namespace detail

#define IMPLEMENT_SPECIFIC_CAST_PATTERN(name, cast_kind)                                           \
  template <typename ValuePattern> auto name(Cast*& instruction, ValuePattern value) {             \
    return cast_specific(instruction, cast_kind, value);                                           \
  }                                                                                                \
                                                                                                   \
  template <typename ValuePattern> auto name(ValuePattern value) {                                 \
    return cast_specific(cast_kind, value);                                                        \
  }

template <typename ValuePattern>
auto cast(Cast*& instruction, CastKind& cast_kind, ValuePattern value) {
  return detail::CastPattern<ValuePattern>(&instruction, &cast_kind, value);
}

template <typename ValuePattern> auto cast(CastKind& cast_kind, ValuePattern value) {
  return detail::CastPattern<ValuePattern>(nullptr, &cast_kind, value);
}

template <typename ValuePattern> auto cast(Cast*& instruction, ValuePattern value) {
  return detail::CastPattern<ValuePattern>(&instruction, nullptr, value);
}

template <typename ValuePattern> auto cast(ValuePattern value) {
  return detail::CastPattern<ValuePattern>(nullptr, nullptr, value);
}

template <typename ValuePattern>
auto cast_specific(Cast*& instruction, CastKind cast_kind, ValuePattern value) {
  return detail::CastPattern<ValuePattern, true>(&instruction, nullptr, value, cast_kind);
}

template <typename ValuePattern> auto cast_specific(CastKind cast_kind, ValuePattern value) {
  return detail::CastPattern<ValuePattern, true>(nullptr, nullptr, value, cast_kind);
}

IMPLEMENT_SPECIFIC_CAST_PATTERN(bitcast, CastKind::Bitcast)
IMPLEMENT_SPECIFIC_CAST_PATTERN(sext, CastKind::SignExtend)
IMPLEMENT_SPECIFIC_CAST_PATTERN(zext, CastKind::ZeroExtend)
IMPLEMENT_SPECIFIC_CAST_PATTERN(trunc, CastKind::Truncate)

#undef IMPLEMENT_SPECIFIC_CAST_PATTERN

} // namespace flugzeug::pat