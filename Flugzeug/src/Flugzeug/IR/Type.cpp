#include "Type.hpp"
#include "Context.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace flugzeug;

Type::Type(Context* context, Type::Kind kind) : context(context), kind(kind) {
  context->increase_refcount();
}

Type::~Type() { context->decrease_refcount(); }

PointerType* Type::ref(uint32_t indirection) const {
  verify(indirection > 0, "Cannnot specify 0 indirection");

  return get_context()->get_pointer_type(const_cast<Type*>(this), indirection);
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
  default:
    unreachable();
  }
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
  case Kind::Void:
    return "void";
  case Kind::Block:
    return "block";
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
  case Kind::Pointer:
  default:
    unreachable();
  }
}

Constant* Type::get_constant(uint64_t constant) { return context->get_constant(this, constant); }
Constant* Type::get_zero() { return get_constant(0); }
Constant* Type::get_one() { return get_constant(1); }

Undef* Type::get_undef() { return context->get_undef(this); }

PointerType::PointerType(Context* context, Type* base, Type* pointee, uint32_t indirection)
    : Type(context, Kind::Pointer), base(base), pointee(pointee), indirection(indirection) {
  if (const auto pointee_p = cast<PointerType>(pointee)) {
    verify(pointee_p->get_indirection() + 1 == indirection, "Invalid pointee");
    verify(pointee_p->get_base() == base, "Invalid base");
  }
}

Type* PointerType::deref() const { return pointee; }