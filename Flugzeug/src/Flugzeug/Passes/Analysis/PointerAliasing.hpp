#pragma once
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/IR/Value.hpp>

namespace flugzeug::analysis {

namespace detail {

class PointerOriginMap {
  std::unordered_map<const Value*, const Value*> map;

public:
  void reserve(size_t size) { map.reserve(size); }

  auto begin() const { return map.begin(); }
  auto end() const { return map.end(); }

  void insert(const Value* value, const Value* origin);
  const Value* get(const Value* value, bool presence_required = true) const;
};

} // namespace detail

enum class Aliasing {
  Never,
  May,
  Always,
};

class PointerAliasing {
  detail::PointerOriginMap pointer_origin_map;
  std::unordered_map<const Value*, bool> stackalloc_safety;
  std::unordered_map<const Value*, std::pair<const Value*, int64_t>> constant_offset_db;

  std::pair<const Value*, int64_t> get_constant_offset(const Value* value) const;

public:
  enum class AccessType {
    Load,
    Store,
    All,
  };

  explicit PointerAliasing(const Function* function);

  Aliasing can_alias(const Instruction* instruction, const Value* v1, const Value* v2) const;
  Aliasing can_instruction_access_pointer(const Instruction* instruction, const Value* pointer,
                                          AccessType access_type) const;

  bool is_pointer_accessed_inbetween(const Value* pointer, const Instruction* begin,
                                     const Instruction* end, AccessType access_type) const;
  bool is_pointer_stackalloc(const Value* pointer) const;
  std::optional<std::pair<const StackAlloc*, int64_t>>
  get_constant_offset_from_stackalloc(const Value* pointer) const;

  void debug_dump() const;
};

} // namespace flugzeug::analysis