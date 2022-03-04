#pragma once
#include "Instructions.hpp"

#include "Patterns/BinaryInstr.hpp"
#include "Patterns/General.hpp"
#include "Patterns/IntCompare.hpp"
#include "Patterns/UnaryInstr.hpp"

namespace flugzeug {

template <typename T, typename Pattern> bool match_pattern(T* value, Pattern pattern) {
  return pattern.match(value);
}

} // namespace flugzeug