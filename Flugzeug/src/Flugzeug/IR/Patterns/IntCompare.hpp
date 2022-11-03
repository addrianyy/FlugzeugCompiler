#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename TInstruction,
          typename LHSPattern,
          typename RHSPattern,
          bool MustBeEqOrNe,
          bool MatchSpecificPred = false>
class IntComparePattern {
  TInstruction** bind_instruction;
  LHSPattern lhs_pattern;
  IntPredicate* bind_pred;
  RHSPattern rhs_pattern;
  IntPredicate specific_pred;

 public:
  IntComparePattern(TInstruction** bind_instruction,
                    LHSPattern lhs,
                    IntPredicate* bind_pred,
                    RHSPattern rhs,
                    IntPredicate specific_pred = IntPredicate::Equal)
      : bind_instruction(bind_instruction),
        lhs_pattern(lhs),
        bind_pred(bind_pred),
        rhs_pattern(rhs),
        specific_pred(specific_pred) {}

  template <typename T>
  bool match(T* m_value) {
    const auto cmp = flugzeug::cast<IntCompare>(m_value);
    if (!cmp) {
      return false;
    }

    const auto pred = cmp->get_pred();
    if (MatchSpecificPred && pred != specific_pred) {
      return false;
    }

    const auto is_eq_or_ne = pred == IntPredicate::Equal || pred == IntPredicate::NotEqual;
    if (MustBeEqOrNe && !is_eq_or_ne) {
      return false;
    }

    const bool matched =
      (lhs_pattern.match(cmp->get_lhs()) && rhs_pattern.match(cmp->get_rhs())) ||
      (is_eq_or_ne && lhs_pattern.match(cmp->get_rhs()) && rhs_pattern.match(cmp->get_lhs()));
    if (!matched) {
      return false;
    }

    if (bind_pred) {
      *bind_pred = pred;
    }

    if (bind_instruction) {
      *bind_instruction = cmp;
    }

    return true;
  }
};

}  // namespace detail

#define IMPLEMENT_INT_COMPARE_PATTERN(name, eq_or_ne)                                         \
  template <typename TInstruction, typename LHSPattern, typename RHSPattern>                  \
  auto name(TInstruction*& instruction, LHSPattern lhs, IntPredicate& pred, RHSPattern rhs) { \
    static_assert(std::is_same_v<IntCompare, std::remove_cv_t<TInstruction>>,                 \
                  "Expected IntCompare instruction in this pattern");                         \
    return detail::IntComparePattern<TInstruction, LHSPattern, RHSPattern, eq_or_ne>(         \
      &instruction, lhs, &pred, rhs);                                                         \
  }                                                                                           \
                                                                                              \
  template <typename LHSPattern, typename RHSPattern>                                         \
  auto name(LHSPattern lhs, IntPredicate& pred, RHSPattern rhs) {                             \
    return detail::IntComparePattern<const IntCompare, LHSPattern, RHSPattern, eq_or_ne>(     \
      nullptr, lhs, &pred, rhs);                                                              \
  }                                                                                           \
                                                                                              \
  template <typename TInstruction, typename LHSPattern, typename RHSPattern>                  \
  auto name(TInstruction*& instruction, LHSPattern lhs, RHSPattern rhs) {                     \
    static_assert(std::is_same_v<IntCompare, std::remove_cv_t<TInstruction>>,                 \
                  "Expected IntCompare instruction in this pattern");                         \
    return detail::IntComparePattern<TInstruction, LHSPattern, RHSPattern, eq_or_ne>(         \
      &instruction, lhs, nullptr, rhs);                                                       \
  }                                                                                           \
                                                                                              \
  template <typename LHSPattern, typename RHSPattern>                                         \
  auto name(LHSPattern lhs, RHSPattern rhs) {                                                 \
    return detail::IntComparePattern<const IntCompare, LHSPattern, RHSPattern, eq_or_ne>(     \
      nullptr, lhs, nullptr, rhs);                                                            \
  }

#define IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(name, pred)                    \
  template <typename TInstruction, typename LHSPattern, typename RHSPattern>  \
  auto name(TInstruction*& instruction, LHSPattern lhs, RHSPattern rhs) {     \
    static_assert(std::is_same_v<IntCompare, std::remove_cv_t<TInstruction>>, \
                  "Expected IntCompare instruction in this pattern");         \
    return compare_specific(instruction, lhs, pred, rhs);                     \
  }                                                                           \
                                                                              \
  template <typename LHSPattern, typename RHSPattern>                         \
  auto name(LHSPattern lhs, RHSPattern rhs) {                                 \
    return compare_specific(lhs, pred, rhs);                                  \
  }

template <typename TInstruction, typename LHSPattern, typename RHSPattern>
auto compare_specific(TInstruction*& instruction,
                      LHSPattern lhs,
                      IntPredicate specific_pred,
                      RHSPattern rhs) {
  static_assert(std::is_same_v<IntCompare, std::remove_cv_t<TInstruction>>,
                "Expected IntCompare instruction in this pattern");
  return detail::IntComparePattern<TInstruction, LHSPattern, RHSPattern, false, true>(
    &instruction, lhs, nullptr, rhs, specific_pred);
}

template <typename LHSPattern, typename RHSPattern>
auto compare_specific(LHSPattern lhs, IntPredicate specific_pred, RHSPattern rhs) {
  return detail::IntComparePattern<const IntCompare, LHSPattern, RHSPattern, false, true>(
    nullptr, lhs, nullptr, rhs, specific_pred);
}

IMPLEMENT_INT_COMPARE_PATTERN(compare, false)
IMPLEMENT_INT_COMPARE_PATTERN(compare_eq_or_ne, true)

IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_eq, IntPredicate::Equal)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_ne, IntPredicate::NotEqual)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_ugt, IntPredicate::GtU)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_ugte, IntPredicate::GteU)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_sgt, IntPredicate::GtS)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_sgte, IntPredicate::GteS)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_ult, IntPredicate::LtU)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_ulte, IntPredicate::LteU)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_slt, IntPredicate::LtS)
IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN(compare_slte, IntPredicate::LteS)

#undef IMPLEMENT_INT_COMPARE_PATTERN
#undef IMPLEMENT_SPECIFIC_INT_COMPARE_PATTERN

}  // namespace flugzeug::pat