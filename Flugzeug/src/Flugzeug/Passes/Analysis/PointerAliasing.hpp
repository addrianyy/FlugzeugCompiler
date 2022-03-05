#pragma once
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/IR/Value.hpp>

namespace flugzeug::analysis {

namespace detail {

class PointerOriginMap {
  std::unordered_map<const Value*, const Value*> map;

public:
  auto begin() const { return map.begin(); }
  auto end() const { return map.end(); }

  void insert(const Value* value, const Value* origin);
  const Value* get(const Value* value, bool presence_required = true) const;
};

} // namespace detail

class PointerAliasing {
  detail::PointerOriginMap pointer_origin_map;
  std::unordered_map<const Value*, bool> stackalloc_safety;
  std::unordered_map<const Value*, std::pair<const Value*, int64_t>> constant_offset_db;

public:
  enum class AccessType {
    Load,
    Store,
    All,
  };

  explicit PointerAliasing(const Function* function);

  bool can_alias(const Value* v1, const Value* v2) const;
  bool can_instruction_access_pointer(const Instruction* instruction, const Value* pointer,
                                      AccessType access_type) const;

  void debug_dump() const;
};

} // namespace flugzeug::analysis