#include "KnownLoadElimination.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <Flugzeug/Passes/Analysis/Paths.hpp>

using namespace flugzeug;

// store X, Y
// ... instructions that don't modify X
// Z = load X
// => last load will be replaced with Y

static bool handle_out_of_bounds_stackalloc_load(Load* load,
                                                 const analysis::PointerAliasing& alias_analysis) {
  const auto const_offset = alias_analysis.get_constant_offset_from_stackalloc(load->address());
  if (!const_offset) {
    return false;
  }

  const auto size = int64_t(const_offset->first->size());
  const auto offset = const_offset->second;
  if (offset < 0 || offset >= size) {
    load->replace_uses_with_and_destroy(load->type()->undef());
    return true;
  }

  return false;
}

bool opt::memory::eliminate_known_loads_local(Function* function,
                                              const analysis::PointerAliasing& alias_analysis) {
  bool did_something = false;

  std::unordered_map<Value*, Store*> stores;
  analysis::PathValidator path_validator;

  for (Block& block : *function) {
    stores.clear();

    for (Instruction& instruction : advance_early(block)) {
      if (const auto store = cast<Store>(instruction)) {
        // The newest known value for the pointer is now defined by this store.
        stores[store->address()] = store;
        continue;
      }

      if (const auto load = cast<Load>(instruction)) {
        if (handle_out_of_bounds_stackalloc_load(load, alias_analysis)) {
          did_something = true;
          continue;
        }

        const auto it = stores.find(load->address());
        if (it == stores.end()) {
          continue;
        }

        // Make sure that nothing inbetween can modify the pointer.
        const auto store = it->second;
        const auto stored_to_inbetween = alias_analysis.is_pointer_accessed_inbetween(
          load->address(), store->next(), load, analysis::PointerAliasing::AccessType::Store);

        if (!stored_to_inbetween) {
          // Alias load instruction with recently stored value.
          load->replace_uses_with_and_destroy(store->value());
          did_something = true;
        }
      }
    }
  }

  return did_something;
}

bool opt::memory::eliminate_known_loads_global(Function* function,
                                               const DominatorTree& dominator_tree,
                                               const analysis::PointerAliasing& alias_analysis) {
  bool did_something = false;

  std::unordered_map<Value*, std::vector<Store*>> stores_to_pointers;
  analysis::PathValidator path_validator;

  // Create a database of all stores in the function.
  for (Store& store : function->instructions<Store>()) {
    stores_to_pointers[store.address()].push_back(&store);
  }

  for (Load& load : advance_early(function->instructions<Load>())) {
    if (handle_out_of_bounds_stackalloc_load(&load, alias_analysis)) {
      did_something = true;
      continue;
    }

    const auto pointer = load.address();
    const auto it = stores_to_pointers.find(pointer);
    if (it == stores_to_pointers.end()) {
      continue;
    }
    const auto& stores = it->second;

    Value* replacement = nullptr;

    for (Store* store : stores) {
      // Check if anything in the path between `store` and `load` can store to the pointer.
      const auto result = path_validator.validate_path(
        dominator_tree, store, &load, analysis::PathValidator::MemoryKillTarget::End,
        [&](const Instruction* instruction) {
          return alias_analysis.can_instruction_access_pointer(
                   instruction, pointer, analysis::PointerAliasing::AccessType::Store) ==
                 analysis::Aliasing::Never;
        });

      if (result) {
        replacement = store->value();
        break;
      }
    }

    if (replacement) {
      load.replace_uses_with_and_destroy(replacement);
      did_something = true;
    }
  }

  return did_something;
}