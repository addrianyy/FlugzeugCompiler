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
  };

private:
  const Kind kind;
  Context* const context;

protected:
  Type(Context* context, Kind kind);

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Type);

  virtual ~Type();

  Context* get_context() const { return context; }
  Kind get_kind() const { return kind; }

  PointerType* ref(uint32_t indirection = 1) const;

  bool is_i1() const { return kind == Kind::I1; }
  bool is_i8() const { return kind == Kind::I8; }
  bool is_i16() const { return kind == Kind::I16; }
  bool is_i32() const { return kind == Kind::I32; }
  bool is_i64() const { return kind == Kind::I64; }
  bool is_void() const { return kind == Kind::Void; }
  bool is_block() const { return kind == Kind::Block; }
  bool is_pointer() const { return kind == Kind::Pointer; }

  bool is_arithmetic() const;
  bool is_arithmetic_or_pointer() const;

  Constant* get_constant(uint64_t constant);
  Constant* get_zero();
  Constant* get_one();

  Undef* get_undef();

  size_t get_bit_size() const;
  uint64_t get_bit_mask() const;

  std::string format() const;
};

class PointerType : public Type {
  DEFINE_INSTANCEOF(Type, Type::Kind::Pointer)

  friend class Context;

  Type* base;
  Type* pointee;
  uint32_t indirection;

  PointerType(Context* context, Type* base, Type* pointee, uint32_t indirection);

public:
  Type* get_base() const { return base; }
  Type* get_pointee() const { return pointee; }

  uint32_t get_indirection() const { return indirection; }

  Type* deref() const;
};

#define DEFINE_SIMPLE_TYPE(type_name, kind_name)                                                   \
  class type_name : public Type {                                                                  \
    DEFINE_INSTANCEOF(Type, Type::Kind::kind_name)                                                 \
                                                                                                   \
    friend class Context;                                                                          \
                                                                                                   \
    explicit type_name(Context* context) : Type(context, Type::Kind::kind_name) {}                 \
  };

DEFINE_SIMPLE_TYPE(I1Type, I1)
DEFINE_SIMPLE_TYPE(I8Type, I8)
DEFINE_SIMPLE_TYPE(I16Type, I16)
DEFINE_SIMPLE_TYPE(I32Type, I32)
DEFINE_SIMPLE_TYPE(I64Type, I64)
DEFINE_SIMPLE_TYPE(BlockType, Block)
DEFINE_SIMPLE_TYPE(VoidType, Void)

#undef DEFINE_SIMPLE_TYPE

} // namespace flugzeug