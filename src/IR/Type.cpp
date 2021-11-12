#include "Type.hpp"
#include <Core/Error.hpp>

using namespace flugzeug;

Type Type::deref() const {
  verify(indirection > 0, "Cannot deref non pointer.");
  return Type(kind, indirection - 1);
}

Type Type::ref() const { return Type(kind, indirection + 1); }

size_t Type::get_bit_size() const {
  verify(kind != Kind::Block && kind != Kind::Void, "Cannot get size of void or block types.");

  if (indirection > 0) {
    return 64;
  }

  return size_t(kind);
}

uint64_t Type::get_bit_mask() const {
  switch (get_bit_size()) {
  case 1:
    return 1ull;

  case 8:
    return 0xffull;

  case 16:
    return 0xffffull;

  case 32:
    return 0xffff'ffffull;

  case 64:
    return 0xffff'ffff'ffff'ffffull;

  default:
    unreachable();
  }
}

std::string Type::format() const {
  std::string result;

  switch (kind) {
  case Kind::Void:
    result = "void";
    break;
  case Kind::I1:
    result = "i1";
    break;
  case Kind::I8:
    result = "i8";
    break;
  case Kind::I16:
    result = "i16";
    break;
  case Kind::I32:
    result = "i32";
    break;
  case Kind::I64:
    result = "i64";
    break;
  case Kind::Block:
    result = "block";
    break;
  }

  result.reserve(result.size() + indirection);
  for (size_t i = 0; i < indirection; ++i) {
    result += '*';
  }

  return result;
}