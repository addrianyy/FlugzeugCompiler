#pragma once
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

bool opt::memory::eliminate_known_loads_local(Function* function,
                                              const analysis::PointerAliasing& alias_analysis) {
  bool did_something = false;

  std::unordered_map<Value*, Store*> stores;
  analysis::PathValidator path_validator;

  for (Block& block : *function) {
    stores.clear();

    for (Instruction& instruction : dont_invalidate_current(block)) {
      if (const auto store = cast<Store>(instruction)) {
        // The newest known value for the pointer is now defined by this store.
        stores[store->get_ptr()] = store;
        continue;
      }

      if (const auto load = cast<Load>(instruction)) {
        const auto it = stores.find(load->get_ptr());
        if (it == stores.end()) {
          continue;
        }

        // Make sure that nothing inbetween can modify the pointer.
        const auto store = it->second;
        const auto stored_to_inbetween = alias_analysis.is_pointer_accessed_inbetween_simple(
          load->get_ptr(), store->get_next(), load, analysis::PointerAliasing::AccessType::Store);

        if (!stored_to_inbetween) {
          // Alias load instruction with recently stored value.
          load->replace_uses_with_and_destroy(store->get_val());
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
    stores_to_pointers[store.get_ptr()].push_back(&store);
  }

  for (Load& load : dont_invalidate_current(function->instructions<Load>())) {
    const auto pointer = load.get_ptr();
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
        replacement = store->get_val();
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