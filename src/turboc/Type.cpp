#include "Type.hpp"

#include <Flugzeug/Core/StringifyEnum.hpp>

using namespace turboc;

// clang-format off
BEGIN_ENUM_STRINGIFY(stringify_type_kind, Type::Kind)
  ENUM_CASE(U8)
  ENUM_CASE(U16)
  ENUM_CASE(U32)
  ENUM_CASE(U64)
  ENUM_CASE(I8)
  ENUM_CASE(I16)
  ENUM_CASE(I32)
  ENUM_CASE(I64)
  ENUM_CASE(Void)
END_ENUM_STRINGIFY()
// clang-format on

std::string turboc::Type::format() const {
  std::string string = std::string(stringify_type_kind(get_kind()));

  string.reserve(string.size() + get_indirection());

  for (uint32_t i = 0; i < get_indirection(); ++i) {
    string += '*';
  }

  return string;
}