#include "PointerAliasing.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/IR/Patterns.hpp>

#include <Flugzeug/Core/HashCombine.hpp>
#include <Flugzeug/Core/Log.hpp>

using namespace flugzeug;
using namespace flugzeug::analysis;

using analysis::detail::PointerOriginMap;

template <typename T> struct ValuePairHash {
  size_t operator()(const std::pair<const Value*, const Value*>& p) const {
    return hash_combine(p.first, p.second);
  }
};

using BaseIndexToOffset = std::unordered_map<std::pair<const Value*, const Value*>, const Offset*,
                                             ValuePairHash<const Value*>>;

template <typename K, typename V>
std::optional<V> lookup_map(const std::unordered_map<K, V>& map, K key) {
  const auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }

  return it->second;
}

void PointerOriginMap::insert(const Value* value, const Value* origin) {
  verify(map.insert({value, origin}).second, "Value was already present in the origin map");
}

const Value* PointerOriginMap::get(const Value* value, bool presence_required) const {
  if (!cast<Instruction>(value)) {
    return value;
  }

  const auto it = map.find(value);
  if (it == map.end()) {
    if (presence_required) {
      fatal_error("Failed to get value origin from the map");
    } else {
      return nullptr;
    }
  }

  return it->second;
}

class PointerOriginCalculator : public ConstInstructionVisitor {
  const PointerOriginMap& origin_map;

  [[noreturn]] void invalid_instruction() { unreachable(); }

public:
  explicit PointerOriginCalculator(const PointerOriginMap& origin_map) : origin_map(origin_map) {}

  // Instructions which can create pointers but for which we don't know the origin.
  const Value* visit_load(Argument<Load> load) { return load; }
  const Value* visit_call(Argument<Call> call) { return call; }

  // Casted pointers can alias, we cannot get their origin.
  const Value* visit_cast(Argument<Cast> cast) { return cast; }

  // We are actually at the primary origin.
  const Value* visit_stackalloc(Argument<StackAlloc> stackalloc) { return stackalloc; }

  const Value* visit_offset(Argument<Offset> offset) { return origin_map.get(offset->get_base()); }
  const Value* visit_select(Argument<Select> select) {
    const auto true_origin = origin_map.get(select->get_true_val());
    const auto false_origin = origin_map.get(select->get_false_val());

    if (true_origin == false_origin) {
      return true_origin;
    }

    return select;
  }
  const Value* visit_phi(Argument<Phi> phi) {
    const Value* common_origin = nullptr;

    for (const auto [_, incoming] : *phi) {
      const auto origin = origin_map.get(incoming, false);
      if (!origin) {
        return phi;
      }

      if (!common_origin) {
        common_origin = origin;
      }

      if (common_origin != origin) {
        return phi;
      }
    }

    return common_origin ? common_origin : phi;
  }

  const Value* visit_unary_instr(Argument<UnaryInstr> unary) { invalid_instruction(); }
  const Value* visit_binary_instr(Argument<BinaryInstr> binary) { invalid_instruction(); }
  const Value* visit_int_compare(Argument<IntCompare> int_compare) { invalid_instruction(); }
  const Value* visit_store(Argument<Store> store) { invalid_instruction(); }
  const Value* visit_branch(Argument<Branch> branch) { invalid_instruction(); }
  const Value* visit_cond_branch(Argument<CondBranch> cond_branch) { invalid_instruction(); }
  const Value* visit_ret(Argument<Ret> ret) { invalid_instruction(); }
};

class PointerSafetyCalculator : public ConstInstructionVisitor {
  const std::unordered_set<const Value*>& safe_pointers;
  const Value* pointer;

public:
  explicit PointerSafetyCalculator(const std::unordered_set<const Value*>& safe_pointers,
                                   const Value* pointer)
      : safe_pointers(safe_pointers), pointer(pointer) {}

  bool visit_store(Argument<Store> store) {
    // Make sure that we are actually storing TO the pointer, not storing the pointer.
    return store->get_ptr() == pointer && store->get_val() != pointer;
  }

  bool visit_load(Argument<Load> load) { return true; }
  bool visit_ret(Argument<Ret> ret) { return true; }
  bool visit_int_compare(Argument<IntCompare> int_compare) { return true; }

  bool visit_offset(Argument<Offset> offset) {
    // Offset returns memory which belongs to the source pointer. Make sure that offset return value
    // is safely used.
    return offset->get_base() == pointer && safe_pointers.contains(offset);
  }

  bool visit_phi(Argument<Phi> phi) {
    // If Phi pointer safety wasn't computed then assume that it is unsafe.
    return safe_pointers.contains(phi);
  }

  bool visit_call(Argument<Call> call) { return false; }
  bool visit_cast(Argument<Cast> cast) { return false; }
  bool visit_stackalloc(Argument<StackAlloc> stackalloc) { return false; }
  bool visit_select(Argument<Select> select) { return false; }
  bool visit_unary_instr(Argument<UnaryInstr> unary) { return false; }
  bool visit_binary_instr(Argument<BinaryInstr> binary) { return false; }
  bool visit_branch(Argument<Branch> branch) { return false; }
  bool visit_cond_branch(Argument<CondBranch> cond_branch) { return false; }
};

static void process_offset_instruction(
  const Offset* offset,
  std::unordered_map<const Value*, std::pair<const Value*, int64_t>>& constant_offset_db,
  BaseIndexToOffset& base_index_to_offset) {

  const auto base = offset->get_base();
  const auto index = offset->get_index();

  if (const auto c_index = cast<Constant>(index)) {
    std::pair result = {base, c_index->get_constant_i()};

    const auto it = constant_offset_db.find(base);
    if (it != constant_offset_db.end()) {
      const auto parent = it->second;
      result = {parent.first, parent.second + c_index->get_constant_i()};
    }

    constant_offset_db.insert({offset, result});
    return;
  }

  base_index_to_offset[std::pair{base, index}] = offset;

  const Value* index_base;
  int64_t index_add;
  if (match_pattern(index, pat::add(pat::value(index_base), pat::constant_i(index_add)))) {
    const Offset* other_offset;
    {
      const auto it = base_index_to_offset.find(std::pair{base, index_base});
      if (it == base_index_to_offset.end()) {
        return;
      }
      other_offset = it->second;
    }

    std::pair result = {other_offset->get_base(), index_add};

    const auto it = constant_offset_db.find(other_offset->get_base());
    if (it != constant_offset_db.end()) {
      const auto parent = it->second;
      result = {parent.first,
                Constant::constrain_i(index_base->get_type(), parent.second + index_add)};
    }

    constant_offset_db.insert({offset, result});
  }
}

PointerAliasing::PointerAliasing(const Function* function) {
  const auto traversal =
    function->get_entry_block()->get_reachable_blocks(TraversalType::DFS_WithStart);

  size_t offset_instruction_count = 0;
  size_t pointer_instruction_count = 0;
  size_t stackalloc_instruction_count = 0;

  std::unordered_set<const Value*> safe_pointers;
  {
    // Reverse ordering so every value is used before being created.
    //
    // We are at point P in the processing order on which value V is created. It is guaranteed that
    // all output values of instructions that use V will after P.
    //
    // After reversing it, it is guaranteed that all output values of instructions that
    // use V are before P and were already processed (which is what we want).
    for (const Block* block : reversed(traversal)) {
      for (const Instruction& instruction : reversed(*block)) {
        if (!instruction.get_type()->is_pointer()) {
          continue;
        }

        pointer_instruction_count++;
        if (const auto offset = cast<Offset>(instruction)) {
          offset_instruction_count++;
        }
        if (const auto stackalloc = cast<StackAlloc>(instruction)) {
          stackalloc_instruction_count++;
        }

        PointerSafetyCalculator safety_calculator(safe_pointers, &instruction);
        bool safe = true;

        // Make sure that all uses are safe. If they are it means that we always know
        // all values which point to the same memory as this pointer. Otherwise, pointer may
        // escape and be accessed by unknown instruction.
        for (const Instruction& user : instruction.users<Instruction>()) {
          safe = visitor::visit_instruction(&user, safety_calculator);
          if (!safe) {
            break;
          }
        }

        if (safe) {
          safe_pointers.insert(&instruction);
        }
      }
    }
  }

  {
    PointerOriginCalculator origin_calculator(pointer_origin_map);
    BaseIndexToOffset base_index_to_offset;

    base_index_to_offset.reserve(offset_instruction_count);
    constant_offset_db.reserve(offset_instruction_count);
    pointer_origin_map.reserve(pointer_instruction_count);
    stackalloc_safety.reserve(stackalloc_instruction_count);

    // Get origin of all pointers used in the function.
    // Save safety of stackallocs.
    // Save constant pointer offsets.
    for (const Block* block : traversal) {
      for (const Instruction& instruction : *block) {
        if (!instruction.get_type()->is_pointer()) {
          continue;
        }

        const auto origin = visitor::visit_instruction(&instruction, origin_calculator);
        verify(origin, "Failed to calculate pointer origin (?)");
        pointer_origin_map.insert(&instruction, origin);

        // Get information about `stackalloc`: check if it's safely used and can't escape.
        if (const auto stackalloc = cast<StackAlloc>(instruction)) {
          const bool safe = safe_pointers.contains(stackalloc);
          stackalloc_safety.insert({stackalloc, safe});
        }

        // Get information about constant pointer offsets.
        if (const auto offset = cast<Offset>(&instruction)) {
          process_offset_instruction(offset, constant_offset_db, base_index_to_offset);
        }
      }
    }
  }
}

Aliasing PointerAliasing::can_alias(const Instruction* instruction, const Value* v1,
                                    const Value* v2) const {
  verify(v1->get_type()->is_pointer() && v2->get_type()->is_pointer(),
         "Provided values aren't pointers");

  // More advanced alias analysis would make use of this instruction.
  (void)instruction;

  // Undefined pointers don't alias anything.
  if (v1->is_undef() || v2->is_undef()) {
    return Aliasing::Never;
  }

  // If two pointers are the same they always alias.
  if (v1 == v2) {
    return Aliasing::Always;
  }

  {
    const auto get_constant_offset = [&](const Value* value) -> std::pair<const Value*, int64_t> {
      const auto it = constant_offset_db.find(value);
      if (it == constant_offset_db.end()) {
        return {value, 0};
      }

      return it->second;
    };

    const auto offset_1 = get_constant_offset(v1);
    const auto offset_2 = get_constant_offset(v2);

    if (offset_1.first == offset_2.first) {
      return offset_1.second != offset_2.second ? Aliasing::Never : Aliasing::Always;
    }
  }

  const auto v1_origin = pointer_origin_map.get(v1);
  const auto v2_origin = pointer_origin_map.get(v2);

  // Undefined pointers don't alias anything.
  if (v1_origin->is_undef() || v2_origin->is_undef()) {
    return Aliasing::Never;
  }

  // If two pointers have the same origin they may alias.
  if (v1_origin == v2_origin) {
    return Aliasing::May;
  }

  const auto v1_stackalloc = lookup_map(stackalloc_safety, v1_origin);
  const auto v2_stackalloc = lookup_map(stackalloc_safety, v2_origin);

  // We don't have any information about pointers. They may alias.
  if (!v1_stackalloc && !v2_stackalloc) {
    return Aliasing::May;
  }

  // Both pointers come from different stackallocs. It doesn't matter if stackallocs are safe -
  // pointers don't alias.
  if (v1_stackalloc && v2_stackalloc) {
    return Aliasing::Never;
  }

  // One pointer comes from stackalloc, other does not. If stackalloc usage is safe then these two
  // pointers can't alias. Otherwise they can.
  if (v1_stackalloc) {
    return *v1_stackalloc ? Aliasing::Never : Aliasing::May;
  }
  if (v2_stackalloc) {
    return *v2_stackalloc ? Aliasing::Never : Aliasing::May;
  }

  unreachable();
}

Aliasing PointerAliasing::can_instruction_access_pointer(const Instruction* instruction,
                                                         const Value* pointer,
                                                         AccessType access_type) const {
  verify(pointer->get_type()->is_pointer(), "Provided value is not a pointer");

  if (access_type == AccessType::Store || access_type == AccessType::All) {
    if (const auto store = cast<Store>(instruction)) {
      return can_alias(store, store->get_ptr(), pointer);
    }
  }

  if (access_type == AccessType::Load || access_type == AccessType::All) {
    if (const auto load = cast<Load>(instruction)) {
      return can_alias(load, load->get_ptr(), pointer);
    }
  }

  if (const auto call = cast<Call>(instruction)) {
    // No pointer can be accessed if this function doesn't take any parameters.
    if (call->get_arg_count() == 0) {
      return Aliasing::Never;
    }

    const auto origin = pointer_origin_map.get(pointer);
    const auto stackalloc = lookup_map(stackalloc_safety, origin);

    if (!stackalloc || *stackalloc != true) {
      // We have no idea about non-safe stackallocs or pointers with unknown origin.
      return Aliasing::May;
    } else {
      // Pointer is a safely used stackalloc. If no argument originates from the same stackalloc,
      // this call cannot affect the pointer.
      for (size_t i = 0; i < call->get_arg_count(); ++i) {
        const auto arg_origin = pointer_origin_map.get(call->get_arg(i));
        if (arg_origin == origin) {
          return Aliasing::May;
        }
      }
    }
  }

  return Aliasing::Never;
}

bool PointerAliasing::is_pointer_accessed_inbetween(const Value* pointer, const Instruction* begin,
                                                    const Instruction* end,
                                                    PointerAliasing::AccessType access_type) const {

  verify(begin->get_block() == end->get_block(), "Instructions are in different blocks");

  for (const Instruction& instruction : instruction_range(begin, end)) {
    if (can_instruction_access_pointer(&instruction, pointer, access_type) != Aliasing::Never) {
      return true;
    }
  }

  return false;
}

bool PointerAliasing::is_pointer_stackalloc(const Value* pointer) const {
  const auto origin = pointer_origin_map.get(pointer);
  return !!lookup_map(stackalloc_safety, origin);
}

void PointerAliasing::debug_dump() const {
  log_debug("Pointer origins:");
  for (auto [pointer, origin] : pointer_origin_map) {
    if (pointer != origin) {
      log_debug("  {}: {}", pointer->format(), origin->format());
    }
  }
  log_debug("");

  log_debug("Stackalloc safety:");
  for (auto [stackalloc, safety] : stackalloc_safety) {
    log_debug("  stackalloc {}: {}", stackalloc->format(), safety ? "safe" : "unsafe");
  }
  log_debug("");

  log_debug("Constant offsets:");
  for (auto [pointer, offset] : constant_offset_db) {
    log_debug("  {} = {} + {}", pointer->format(), offset.first->format(), offset.second);
  }
  log_debug("");
}
