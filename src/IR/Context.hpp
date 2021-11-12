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

  std::unordered_map<ConstantKey, Constant*, ConstantKeyHash> constants;
  std::unordered_map<Type, Undef*, TypeHash> undefs;

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Context)

  Context() = default;
  ~Context();

  Constant* get_constant(Type type, uint64_t constant);
  Undef* get_undef(Type type);

  Function* create_function(Type return_type, std::string name, const std::vector<Type>& arguments);
};

} // namespace flugzeug