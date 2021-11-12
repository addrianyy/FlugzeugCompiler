#include "Type.hpp"
#include "Context.hpp"
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

TypeX::TypeX(Context* context, TypeX::Kind kind) : context(context), kind(kind) {
  context->increase_refcount();
}

TypeX::~TypeX() { context->decrease_refcount(); }

PointerType* TypeX::ref() const {
  return get_context()->create_pointer_type(const_cast<TypeX*>(this), 1);
}

size_t TypeX::get_bit_size() const {
  switch (kind) {
  case Kind::Void:
  case Kind::Block:
    fatal_error("Cannot get size of void or block types");
  case Kind::I1:
    return 1;
  case Kind::I8:
    return 8;
  case Kind::I16:
    return 16;
  case Kind::I32:
    return 32;
  case Kind::I64:
  case Kind::Pointer:
    return 64;
  }
}

uint64_t TypeX::get_bit_mask() const {
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

std::string TypeX::format() const {
  std::string result;

  if (const auto pointer = cast<PointerType>(this)) {
    result = pointer->get_base()->format();

    result.reserve(result.size() + pointer->get_indirection());
    for (size_t i = 0; i < pointer->get_indirection(); ++i) {
      result += '*';
    }

    return result;
  }

  switch (kind) {
  case Kind::Pointer:
    unreachable();
  case Kind::Void:
    return "void";
  case Kind::I1:
    return "i1";
  case Kind::I8:
    return "i8";
  case Kind::I16:
    return "i16";
  case Kind::I32:
    return "i32";
  case Kind::I64:
    return "i64";
  case Kind::Block:
    return "block";
  }
}

PointerType::PointerType(Context* context, TypeX* base, TypeX* pointee, uint32_t indirection)
    : TypeX(context, Kind::Pointer), base(base), pointee(pointee), indirection(indirection) {
  if (const auto pointee_p = cast<PointerType>(pointee)) {
    verify(pointee_p->get_indirection() + 1 == indirection, "Invalid pointee");
    verify(pointee_p->get_base() == base, "Invalid base");
  }
}

TypeX* PointerType::deref() const { return pointee; }