#pragma once
#include "DeadStoreElimination.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

// store X, Y
// ... instructions that don't modify X
// store X, Z
// => first store will be removed

bool opt::memory::eliminate_dead_stores_local(Function* function,
                                              const analysis::PointerAliasing& alias_analysis) {
  bool did_something = false;

  std::unordered_map<Value*, Store*> stores;

  for (Block& block : *function) {
    stores.clear();

    for (Store& store : dont_invalidate_current(block.instructions<Store>())) {
      Store* previous_store = nullptr;
      {
        const auto it = stores.find(store.get_ptr());
        if (it != stores.end()) {
          previous_store = it->second;
        }
      }

      // Update the latest store to this pointer.
      stores[store.get_ptr()] = &store;

      if (!previous_store) {
        continue;
      }

      // Make sure that nothing inbetween can load the pointer.
      const auto loaded_inbetween = alias_analysis.is_pointer_accessed_inbetween(
        store.get_ptr(), previous_store->get_next(), &store,
        analysis::PointerAliasing::AccessType::Load);

      // Destroy previous store as it's dead.
      if (!loaded_inbetween) {
        previous_store->destroy();
        did_something = true;
      }
    }
  }

  return did_something;
}

bool opt::memory::eliminate_dead_stores_global(Function* function,
                                               const analysis::PointerAliasing& alias_analysis) {
  fatal_error("TODO");
}