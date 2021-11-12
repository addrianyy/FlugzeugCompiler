#include "Type.hpp"
#include "Context.hpp"
#include <Core/Error.hpp>

using namespace flugzeug;

Type::Type(Context* context, Type::Kind kind) : context(context), kind(kind) {
  context->increase_refcount();
}

Type::~Type() { context->decrease_refcount(); }

PointerType* Type::ref(uint32_t indirection) const {
  verify(indirection > 0, "Cannnot specify 0 indirection");

  return get_context()->create_pointer_type(const_cast<Type*>(this), indirection);
}

size_t Type::get_bit_size() const {
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

  unreachable();
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

  unreachable();
}

PointerType::PointerType(Context* context, Type* base, Type* pointee, uint32_t indirection)
    : Type(context, Kind::Pointer), base(base), pointee(pointee), indirection(indirection) {
  if (const auto pointee_p = cast<PointerType>(pointee)) {
    verify(pointee_p->get_indirection() + 1 == indirection, "Invalid pointee");
    verify(pointee_p->get_base() == base, "Invalid base");
  }
}

Type* PointerType::deref() const { return pointee; }