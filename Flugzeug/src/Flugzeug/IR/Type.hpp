#pragma once
#include <Flugzeug/Core/Casting.hpp>
#include <Flugzeug/Core/ClassTraits.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace flugzeug {

class Context;
class Constant;
class Undef;
class PointerType;

class Type {
 public:
  enum class Kind {
    Void,
    Block,
    I1,
    I8,
    I16,
    I32,
    I64,
    Pointer,
    Function,
  };

 private:
  const Kind kind_;
  Context* const context_;

  mutable PointerType* pointer_to_this = nullptr;

  mutable Constant* zero_ = nullptr;
  mutable Constant* one_ = nullptr;
  mutable Undef* undef_ = nullptr;

 protected:
  Type(Context* context, Kind kind);

 public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Type);

  virtual ~Type();

  Context* context() const { return context_; }
  Kind get_kind() const { return kind_; }

  PointerType* ref(uint32_t indirection = 1) const;

  bool is_i1() const { return kind_ == Kind::I1; }
  bool is_i8() const { return kind_ == Kind::I8; }
  bool is_i16() const { return kind_ == Kind::I16; }
  bool is_i32() const { return kind_ == Kind::I32; }
  bool is_i64() const { return kind_ == Kind::I64; }
  bool is_void() const { return kind_ == Kind::Void; }
  bool is_block() const { return kind_ == Kind::Block; }
  bool is_function() const { return kind_ == Kind::Function; }
  bool is_pointer() const { return kind_ == Kind::Pointer; }

  bool is_arithmetic() const;
  bool is_arithmetic_or_pointer() const;

  Constant* constant(uint64_t constant) const;
  Constant* zero() const;
  Constant* one() const;

  Undef* undef() const;

  size_t bit_size() const;
  size_t byte_size() const;
  uint64_t bit_mask() const;

  std::string format() const;
};

class PointerType final : public Type {
  DEFINE_INSTANCEOF(Type, Type::Kind::Pointer)

  friend class Context;

  Type* base_;
  Type* pointee_;
  uint32_t indirection_;

  PointerType(Context* context, Type* base, Type* pointee, uint32_t indirection);

 public:
  Type* base_type() const { return base_; }
  Type* pointee() const { return pointee_; }

  uint32_t indirection() const { return indirection_; }

  Type* deref() const { return pointee_; }
};

#define DEFINE_SIMPLE_TYPE(type_name, kind_name)                                   \
  class type_name final : public Type {                                            \
    DEFINE_INSTANCEOF(Type, Type::Kind::kind_name)                                 \
                                                                                   \
    friend class Context;                                                          \
                                                                                   \
    explicit type_name(Context* context) : Type(context, Type::Kind::kind_name) {} \
  };

DEFINE_SIMPLE_TYPE(I1Type, I1)
DEFINE_SIMPLE_TYPE(I8Type, I8)
DEFINE_SIMPLE_TYPE(I16Type, I16)
DEFINE_SIMPLE_TYPE(I32Type, I32)
DEFINE_SIMPLE_TYPE(I64Type, I64)
DEFINE_SIMPLE_TYPE(VoidType, Void)
DEFINE_SIMPLE_TYPE(BlockType, Block)
DEFINE_SIMPLE_TYPE(FunctionType, Function)

#undef DEFINE_SIMPLE_TYPE

}  // namespace flugzeug