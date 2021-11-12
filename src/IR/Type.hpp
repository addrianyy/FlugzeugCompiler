#pragma once
#include <Core/Casting.hpp>
#include <Core/ClassTraits.hpp>
#include <cstdint>
#include <string>

namespace flugzeug {

class Type {
public:
  enum class Kind {
    Void = 0,
    I1 = 1,
    I8 = 8,
    I16 = 16,
    I32 = 32,
    I64 = 64,
    Block = 1024,
  };

private:
  Kind kind;
  uint32_t indirection;

public:
  Type(Kind kind, uint32_t indirection = 0) : kind(kind), indirection(indirection) {}

  bool is_pointer() const { return indirection > 0; }

  Kind get_kind() const { return kind; }
  uint32_t get_indirection() const { return indirection; }

  Type deref() const;
  Type ref() const;

  size_t get_bit_size() const;
  uint64_t get_bit_mask() const;

  bool operator==(const Type& other) const {
    return kind == other.kind && indirection == other.indirection;
  }
  bool operator==(const Kind other) const { return kind == other && indirection == 0; }

  bool is_void() const { return kind == Kind::Void; }

  std::string format() const;
};

class Context;
class PointerType;

class TypeX {
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
  TypeX(Context* context, Kind kind);

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(TypeX);

  virtual ~TypeX();

  Context* get_context() const { return context; }
  Kind get_kind() const { return kind; }

  PointerType* ref() const;

  bool is_void() const { return kind == Kind::Void; }
  bool is_pointer() const { return kind == Kind::Pointer; }

  size_t get_bit_size() const;
  uint64_t get_bit_mask() const;

  std::string format() const;
};

class PointerType : public TypeX {
  DEFINE_INSTANCEOF(TypeX, TypeX::Kind::Pointer)

  friend class Context;

  TypeX* base;
  TypeX* pointee;
  uint32_t indirection;

  PointerType(Context* context, TypeX* base, TypeX* pointee, uint32_t indirection);

public:
  TypeX* get_base() const { return base; }
  TypeX* get_pointee() const { return pointee; }

  uint32_t get_indirection() const { return indirection; }

  TypeX* deref() const;
};

#define DEFINE_SIMPLE_TYPE(type_name, kind_name)                                                   \
  class type_name : public TypeX {                                                                 \
    DEFINE_INSTANCEOF(TypeX, TypeX::Kind::kind_name)                                               \
                                                                                                   \
    friend class Context;                                                                          \
                                                                                                   \
    explicit type_name(Context* context) : TypeX(context, TypeX::Kind::kind_name) {}               \
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