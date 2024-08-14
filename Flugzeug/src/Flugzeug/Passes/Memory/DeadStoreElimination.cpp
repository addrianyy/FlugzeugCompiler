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

    for (Store& store : advance_early(block.instructions<Store>())) {
      Store* previous_store = nullptr;
      {
        const auto it = stores.find(store.address());
        if (it != stores.end()) {
          previous_store = it->second;
        }
      }

      // Update the latest store to this pointer.
      stores[store.address()] = &store;

      if (!previous_store) {
        continue;
      }

      // Make sure that nothing inbetween can load the pointer.
      const auto loaded_inbetween = alias_analysis.is_pointer_accessed_inbetween(
        store.address(), previous_store->next(), &store,
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

static bool is_store_dead(const Store* store, const analysis::PointerAliasing& alias_analysis) {
  enum class CheckResult {
    Ok,
    Invalid,
    CheckSuccessors,
  };

  const auto pointer = store->address();

  const auto check_instruction_range = [&](const Instruction* begin, const Instruction* end) {
    for (const Instruction& instruction : instruction_range(begin, end)) {
      // If there is another store to this pointer then no successors can observe the old value.
      if (const auto store = cast<Store>(instruction)) {
        if (alias_analysis.can_alias(store, store->address(), pointer) ==
            analysis::Aliasing::Always) {
          return CheckResult::Ok;
        }
      }

      // If this value can be observed we need to back down.
      if (alias_analysis.can_instruction_access_pointer(
            &instruction, pointer, analysis::PointerAliasing::AccessType::Load) !=
          analysis::Aliasing::Never) {
        return CheckResult::Invalid;
      }
    }

    // Nothing here can observe the pointer but block's successors may.
    return CheckResult::CheckSuccessors;
  };

  const auto check_block = [&](const Block* block) -> CheckResult {
    return check_instruction_range(block->first_instruction(), nullptr);
  };

  {
    // Make sure that nothing in the remaining part of the store block can observe the pointer.
    const auto result = check_instruction_range(store->next(), nullptr);
    if (result == CheckResult::Ok) {
      return true;
    }
    if (result == CheckResult::Invalid) {
      return false;
    }
  }

  std::unordered_set<const Block*> visited;
  std::vector<const Block*> stack;

  // Start from store block and traverse down.
  stack.push_back(store->block());

  bool can_reach_itself = false;

  while (!stack.empty()) {
    const auto block = stack.back();
    stack.pop_back();

    if (!visited.insert(block).second) {
      continue;
    }

    if (block != store->block()) {
      const auto result = check_block(block);
      if (result == CheckResult::Ok) {
        continue;
      }
      if (result == CheckResult::Invalid) {
        return false;
      }
    }

    const auto successors = block->successors();

    // If block ends with `ret` and `pointer` is not coming from stackalloc than `store`
    // can be observed by the caller.
    if (successors.empty() && !alias_analysis.is_pointer_stackalloc(pointer)) {
      return false;
    }

    for (const auto successor : successors) {
      if (successor == store->block()) {
        can_reach_itself = true;
        continue;
      }

      if (!visited.contains(successor)) {
        stack.push_back(successor);
      }
    }
  }

  if (can_reach_itself) {
    // Store block can reach itself. Make sure that nothing in the initial part of the store block
    // can observe the pointer.
    const auto first_instruction = store->block()->first_instruction();
    if (check_instruction_range(first_instruction, store) == CheckResult::Invalid) {
      return false;
    }
  }

  return true;
}

bool opt::memory::eliminate_dead_stores_global(Function* function,
                                               const DominatorTree& dominator_tree,
                                               const analysis::PointerAliasing& alias_analysis) {
  bool did_something = false;

  for (Store& store : advance_early(function->instructions<Store>())) {
    // Remove this store if it's dead.
    if (is_store_dead(&store, alias_analysis)) {
      store.destroy();
      did_something = true;
    }
  }

  return did_something;
}