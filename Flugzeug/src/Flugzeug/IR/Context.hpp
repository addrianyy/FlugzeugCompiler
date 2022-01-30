#pragma once
#include "Type.hpp"

#include <Flugzeug/Core/ClassTraits.hpp>

#include <cstdint>
#include <unordered_map>

namespace flugzeug {

class Function;
class Constant;
class Undef;

class Context {
  friend class Function;
  friend class Value;
  friend class Type;
  friend class Module;

  int64_t refcount = 0;

  void increase_refcount();
  void decrease_refcount();

  struct ConstantKey {
    Type* type;
    uint64_t constant;

    bool operator==(const ConstantKey& other) const {
      return constant == other.constant && type == other.type;
    }
  };

  struct ConstantKeyHash {
    size_t operator()(const ConstantKey& p) const;
  };

  struct PointerKey {
    Type* base;
    uint32_t indirection;

    bool operator==(const PointerKey& other) const {
      return base == other.base && indirection == other.indirection;
    }
  };

  struct PointerKeyHash {
    size_t operator()(const PointerKey& p) const;
  };

  std::unordered_map<ConstantKey, Constant*, ConstantKeyHash> constants;
  std::unordered_map<Type*, Undef*> undefs;

  std::unordered_map<PointerKey, PointerType*, PointerKeyHash> pointer_types;

  I1Type* i1_type = nullptr;
  I8Type* i8_type = nullptr;
  I16Type* i16_type = nullptr;
  I32Type* i32_type = nullptr;
  I64Type* i64_type = nullptr;
  VoidType* void_type = nullptr;
  BlockType* block_type = nullptr;
  FunctionType* function_type = nullptr;

  PointerType* get_pointer_type_internal(Type* base, uint32_t indirection);

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Context)

  Context();
  ~Context();

  Type* get_i1_ty() const { return i1_type; }
  Type* get_i8_ty() const { return i8_type; }
  Type* get_i16_ty() const { return i16_type; }
  Type* get_i32_ty() const { return i32_type; }
  Type* get_i64_ty() const { return i64_type; }
  Type* get_void_ty() const { return void_type; }
  Type* get_block_ty() const { return block_type; }
  Type* get_function_ty() const { return function_type; }

  Constant* get_constant(Type* type, uint64_t constant);
  Undef* get_undef(Type* type);

  PointerType* get_pointer_type(Type* pointee, uint32_t indirection = 1);

  Module* create_module();
};

} // namespace flugzeug