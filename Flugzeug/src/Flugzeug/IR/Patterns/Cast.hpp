#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename TInstruction, typename ValuePattern, bool MatchSpecificCast = false>
class CastPattern {
  TInstruction** bind_instruction;
  CastKind* bind_kind;
  ValuePattern value;
  CastKind specific_kind;

public:
  CastPattern(TInstruction** bind_instruction, CastKind* bind_kind, ValuePattern value,
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
  template <typename TInstruction, typename ValuePattern>                                          \
  auto name(TInstruction*& instruction, ValuePattern value) {                                      \
    static_assert(std::is_same_v<Cast, std::remove_cv_t<TInstruction>>,                            \
                  "Expected Cast instruction in this pattern");                                    \
    return cast_specific(instruction, cast_kind, value);                                           \
  }                                                                                                \
                                                                                                   \
  template <typename ValuePattern> auto name(ValuePattern value) {                                 \
    return cast_specific(cast_kind, value);                                                        \
  }

template <typename TInstruction, typename ValuePattern>
auto cast(TInstruction*& instruction, CastKind& cast_kind, ValuePattern value) {
  static_assert(std::is_same_v<Cast, std::remove_cv_t<TInstruction>>,
                "Expected Cast instruction in this pattern");
  return detail::CastPattern<TInstruction, ValuePattern>(&instruction, &cast_kind, value);
}

template <typename ValuePattern> auto cast(CastKind& cast_kind, ValuePattern value) {
  return detail::CastPattern<const Cast, ValuePattern>(nullptr, &cast_kind, value);
}

template <typename TInstruction, typename ValuePattern>
auto cast(TInstruction*& instruction, ValuePattern value) {
  static_assert(std::is_same_v<Cast, std::remove_cv_t<TInstruction>>,
                "Expected Cast instruction in this pattern");
  return detail::CastPattern<TInstruction, ValuePattern>(&instruction, nullptr, value);
}

template <typename ValuePattern> auto cast(ValuePattern value) {
  return detail::CastPattern<const Cast, ValuePattern>(nullptr, nullptr, value);
}

template <typename TInstruction, typename ValuePattern>
auto cast_specific(TInstruction*& instruction, CastKind cast_kind, ValuePattern value) {
  static_assert(std::is_same_v<Cast, std::remove_cv_t<TInstruction>>,
                "Expected Cast instruction in this pattern");
  return detail::CastPattern<TInstruction, ValuePattern, true>(&instruction, nullptr, value,
                                                               cast_kind);
}

template <typename ValuePattern> auto cast_specific(CastKind cast_kind, ValuePattern value) {
  return detail::CastPattern<const Cast, ValuePattern, true>(nullptr, nullptr, value, cast_kind);
}

IMPLEMENT_SPECIFIC_CAST_PATTERN(bitcast, CastKind::Bitcast)
IMPLEMENT_SPECIFIC_CAST_PATTERN(sext, CastKind::SignExtend)
IMPLEMENT_SPECIFIC_CAST_PATTERN(zext, CastKind::ZeroExtend)
IMPLEMENT_SPECIFIC_CAST_PATTERN(trunc, CastKind::Truncate)

#undef IMPLEMENT_SPECIFIC_CAST_PATTERN

} // namespace flugzeug::pat