#pragma once
#include <type_traits>

namespace flugzeug::utils {

template <typename TInstruction, typename T1, typename T2>
bool get_commutative_operation_operands(TInstruction* instruction, T1& v1, T2& v2) {
  using T1Base = std::remove_pointer_t<std::remove_cvref_t<T1>>;
  using T2Base = std::remove_pointer_t<std::remove_cvref_t<T2>>;

  v1 = cast<T1Base>(instruction->get_lhs());
  v2 = cast<T2Base>(instruction->get_rhs());
  if (!v1 || !v2) {
    v1 = cast<T1Base>(instruction->get_rhs());
    v2 = cast<T2Base>(instruction->get_lhs());
    if (!v1 || !v2) {
      v1 = nullptr;
      v2 = nullptr;

      return false;
    }
  }

  return true;
}

} // namespace flugzeug::utils