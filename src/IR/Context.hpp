#pragma once
#include "Type.hpp"
#include <Core/ClassTraits.hpp>

#include <cstdint>
#include <unordered_map>

namespace flugzeug {

class Function;
class Constant;
class Undef;

class Context {
  friend class Function;
  friend class Value;
  friend class TypeX;

  int64_t refcount = 0;

  void increase_refcount();
  void decrease_refcount();

  struct ConstantKey {
    Type type;
    uint64_t constant;

    bool operator==(const ConstantKey& other) const {
      return constant == other.constant && type == other.type;
    }
  };

  struct ConstantKeyHash {
    size_t operator()(const ConstantKey& p) const;
  };

  struct TypeHash {
    size_t operator()(const Type& p) const;
  };

  struct PointerKey {
    TypeX* base;
    uint32_t indirection;

    bool operator==(const PointerKey& other) const {
      return base == other.base && indirection == other.indirection;
    }
  };

  struct PointerKeyHash {
    size_t operator()(const PointerKey& p) const;
  };

  std::unordered_map<ConstantKey, Constant*, ConstantKeyHash> constants;
  std::unordered_map<Type, Undef*, TypeHash> undefs;

  std::unordered_map<PointerKey, PointerType*, PointerKeyHash> pointer_types;

  I1Type* i1_type = nullptr;
  I8Type* i8_type = nullptr;
  I16Type* i16_type = nullptr;
  I32Type* i32_type = nullptr;
  I64Type* i64_type = nullptr;
  BlockType* block_type = nullptr;
  VoidType* void_type = nullptr;

  PointerType* create_pointer_type_internal(TypeX* base, uint32_t indirection);

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Context)

  Context();
  ~Context();

  I1Type* get_i1_ty() const { return i1_type; }
  I8Type* get_i8_ty() const { return i8_type; }
  I16Type* get_i16_ty() const { return i16_type; }
  I32Type* get_i32_ty() const { return i32_type; }
  I64Type* get_i64_ty() const { return i64_type; }
  BlockType* get_block_ty() const { return block_type; }
  VoidType* get_void_ty() const { return void_type; }

  Constant* get_constant(Type type, uint64_t constant);
  Undef* get_undef(Type type);

  PointerType* create_pointer_type(TypeX* pointee, uint32_t indirection = 1);

  Function* create_function(Type return_type, std::string name, const std::vector<Type>& arguments);
};

} // namespace flugzeug