#pragma once
#include "DeadStoreElimination.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <Flugzeug/Passes/Analysis/Paths.hpp>

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

static bool can_remove_store(Block* killed_store_block, Block* killer_store_block, Value* pointer,
                             const analysis::PointerAliasing& alias_analysis) {
  enum class CheckBlockResult {
    Ok,
    Invalid,
    CheckSuccessors,
  };

  const auto check_block = [&](Block* block) -> CheckBlockResult {
    for (Instruction& instruction : *block) {
      // If there is another store to this pointer than no successors can observe old value.
      if (const auto store = cast<Store>(instruction)) {
        if (alias_analysis.can_alias(store, store->get_ptr(), pointer) ==
            analysis::Aliasing::Always) {
          return CheckBlockResult::Ok;
        }
      }

      // If this value can be observed we need to back down.
      if (alias_analysis.can_instruction_access_pointer(
            &instruction, pointer, analysis::PointerAliasing::AccessType::Load) !=
          analysis::Aliasing::Never) {
        return CheckBlockResult::Invalid;
      }
    }

    // Nothing here can observe the pointer but block's successors may.
    return CheckBlockResult::CheckSuccessors;
  };

  std::unordered_set<Block*> visited;
  std::vector<Block*> stack;

  // Start from `killed_store_block` and traverse down.
  stack.push_back(killed_store_block);

  while (!stack.empty()) {
    const auto block = stack.back();
    stack.pop_back();

    if (!visited.insert(block).second) {
      continue;
    }

    if (block != killed_store_block) {
      const auto result = check_block(block);

      if (result == CheckBlockResult::Ok) {
        continue;
      }

      if (result == CheckBlockResult::Invalid) {
        return false;
      }
    }

    auto successors = block->successors();

    // If block ends with `ret` and `pointer` is not coming from stackalloc than `store`
    // can be observed by the caller.
    if (successors.empty() && !alias_analysis.is_pointer_stackalloc(pointer)) {
      return false;
    }

    for (const auto successor : successors) {
      // Stop going down if we hit `killer_store_block`.
      if (successor == killer_store_block || visited.contains(successor)) {
        continue;
      }

      stack.push_back(successor);
    }
  }

  return true;
}

static bool remove_one_store(std::vector<Store*>& stores, const DominatorTree& dominator_tree,
                             const analysis::PointerAliasing& alias_analysis,
                             analysis::PathValidator& path_validator) {
  if (stores.size() <= 1) {
    return false;
  }

  const auto pointer = stores.front()->get_ptr();

  // Go through all store combinations and find which one can be removed.
  for (Store* killed_store : stores) {
    for (Store* killer_store : stores) {
      if (killed_store == killer_store) {
        continue;
      }

      // Fix for CLion bug.
      const auto pointer_2 = pointer;

      // Path will go from `killed_store` to `killer_store`. Make sure that there is nothing
      // inbetween that can load our pointer. If there is something, we can't eliminate the store.
      const auto result = path_validator.validate_path(
        dominator_tree, killed_store, killer_store,
        analysis::PathValidator::MemoryKillTarget::Start, [&](const Instruction* instruction) {
          return alias_analysis.can_instruction_access_pointer(
                   instruction, pointer_2, analysis::PointerAliasing::AccessType::Load) ==
                 analysis::Aliasing::Never;
        });

      if (!result) {
        continue;
      }

      // Make sure that removing this store won't have any side effects.
      if (!can_remove_store(killed_store->get_block(), killer_store->get_block(), pointer,
                            alias_analysis)) {
        continue;
      }

      // Remove store from the list and destroy it.
      stores.erase(std::remove(begin(stores), end(stores), killed_store), end(stores));
      killed_store->destroy();

      return true;
    }
  }

  return false;
}

bool opt::memory::eliminate_dead_stores_global(Function* function,
                                               const DominatorTree& dominator_tree,
                                               const analysis::PointerAliasing& alias_analysis) {
  bool did_something = false;

  std::unordered_map<Value*, std::vector<Store*>> stores_to_pointers;
  analysis::PathValidator path_validator;

  // Create a database of all stores in the function.
  for (Store& store : function->instructions<Store>()) {
    stores_to_pointers[store.get_ptr()].push_back(&store);
  }

  for (auto& [pointer, stores] : stores_to_pointers) {
    // Keep removing one store from the list as long as we were successful in the previous
    // iteration.
    while (remove_one_store(stores, dominator_tree, alias_analysis, path_validator)) {
      did_something = true;
    }
  }

  return did_something;
}