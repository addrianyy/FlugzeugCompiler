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

Type Type::strip_pointer() const {
  verify(indirection > 0, "Cannot strip non-pointer values");
  return Type(kind, indirection - 1);
}

Type Type::add_pointer() const {
  return Type(kind, indirection + 1);
}

bool Type::is_signed() const {
  return kind == Kind::I8 || kind == Kind::I16 || kind == Kind::I32 || kind == Kind::I64;
}

size_t Type::get_byte_size() const {
  if (is_pointer()) {
    return 8;
  }

  switch (kind) {
    case Kind::U8:
    case Kind::I8:
      return 1;

    case Kind::U16:
    case Kind::I16:
      return 2;

    case Kind::U32:
    case Kind::I32:
      return 4;

    case Kind::U64:
    case Kind::I64:
      return 8;

    case Kind::Void:
      fatal_error("Cannot get size of void type");

    default:
      unreachable();
  }
}

std::string turboc::Type::format() const {
  std::string string = std::string(stringify_type_kind(get_kind()));

  string.reserve(string.size() + get_indirection());

  for (uint32_t i = 0; i < get_indirection(); ++i) {
    string += '*';
  }

  return string;
}
