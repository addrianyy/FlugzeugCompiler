#include "Type.hpp"
#include "Context.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace flugzeug;

Type::Type(Context* context, Kind kind) : context_(context), kind_(kind) {
  context_->increase_refcount();
}

Type::~Type() {
  context_->decrease_refcount();
}

PointerType* Type::ref(uint32_t indirection) const {
  verify(indirection > 0, "Cannnot specify no indirection");

  const auto non_const = const_cast<Type*>(this);

  if (indirection == 1) {
    if (!pointer_to_this) {
      pointer_to_this = context()->pointer_type(non_const, 1);
    }

    return pointer_to_this;
  }

  return context()->pointer_type(non_const, indirection);
}

size_t Type::bit_size() const {
  switch (kind_) {
    case Kind::Void:
    case Kind::Block:
    case Kind::Function:
      fatal_error("Cannot get size of void or block or function types");
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

size_t Type::byte_size() const {
  const auto bit_size_ = bit_size();
  verify(bit_size_ % 8 == 0, "Bit size is not divisible by 8");
  return bit_size_ / 8;
}

uint64_t Type::bit_mask() const {
  switch (bit_size()) {
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
    result = pointer->base_type()->format();

    result.reserve(result.size() + pointer->indirection());
    for (size_t i = 0; i < pointer->indirection(); ++i) {
      result += '*';
    }

    return result;
  }

  switch (kind_) {
    case Kind::Void:
      return "void";
    case Kind::Block:
      return "block";
    case Kind::Function:
      return "function";
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

Constant* Type::constant(uint64_t constant) const {
  const auto non_const = const_cast<Type*>(this);

  if (constant == 0) {
    if (!zero_) {
      zero_ = context_->make_constant(non_const, 0);
    }

    return zero_;
  }

  if (constant == 1) {
    if (!one_) {
      one_ = context_->make_constant(non_const, 1);
    }

    return one_;
  }

  return context_->make_constant(non_const, constant);
}

Constant* Type::zero() const {
  return constant(0);
}
Constant* Type::one() const {
  return constant(1);
}

Undef* Type::undef() const {
  if (!undef_) {
    undef_ = context_->make_undef(const_cast<Type*>(this));
  }

  return undef_;
}

bool Type::is_arithmetic() const {
  return kind_ == Kind::I8 || kind_ == Kind::I16 || kind_ == Kind::I32 || kind_ == Kind::I64;
}

bool Type::is_arithmetic_or_pointer() const {
  return is_pointer() || is_arithmetic();
}

PointerType::PointerType(Context* context, Type* base, Type* pointee, uint32_t indirection)
    : Type(context, Kind::Pointer), base_(base), pointee_(pointee), indirection_(indirection) {
  if (const auto pointee_p = cast<PointerType>(pointee)) {
    verify(pointee_p->indirection() + 1 == indirection, "Invalid pointee");
    verify(pointee_p->base_type() == base, "Invalid base");
  }
}