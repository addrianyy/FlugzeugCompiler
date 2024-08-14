#include "LoopMemoryExtraction.hpp"
#include "Analysis/Loops.hpp"
#include "Analysis/PointerAliasing.hpp"
#include "Flugzeug/IR/DominatorTree.hpp"
#include "Utils/LoopTransforms.hpp"

#include <algorithm>

using namespace flugzeug;

struct MemoryDfsContext {
  std::vector<Block*> stack;
  std::unordered_set<Block*> visited;
};

static Value* get_load_store_pointer(Instruction* instruction) {
  if (const auto load = cast<Load>(instruction)) {
    return load->get_ptr();
  }
  if (const auto store = cast<Store>(instruction)) {
    return store->get_ptr();
  }

  return nullptr;
}

/// Return true if this loop will always execute at least one of the instructions in `loads_stores`
/// before exiting.
static bool is_memory_access_unconditional(MemoryDfsContext& dfs_context,
                                           const analysis::Loop* loop,
                                           const std::unordered_set<Instruction*>& loads_stores) {
  dfs_context.stack.clear();
  dfs_context.visited.clear();

  dfs_context.stack.push_back(loop->get_header());

  while (!dfs_context.stack.empty()) {
    const auto block = dfs_context.stack.back();
    dfs_context.stack.pop_back();

    if (!dfs_context.visited.insert(block).second) {
      continue;
    }

    // If some instruction here accesses the pointer we don't need to go down.
    const bool accessed =
      any_of(*block, [&](Instruction& instruction) { return loads_stores.contains(&instruction); });
    if (accessed) {
      continue;
    }

    for (Block* successor : block->successors()) {
      // If we have exited the loop then memory access is not unconditional.
      if (!loop->contains_block(successor)) {
        return false;
      }

      if (!dfs_context.visited.contains(successor)) {
        dfs_context.stack.push_back(successor);
      }
    }
  }

  return true;
}

/// Rewrite all uses of the pointer to use newly created stackalloc instead. Load/store the actual
/// pointer only on the loop boundaries and around aliasing calls.
static void rewrite_pointer(Value* pointer,
                            Block* preheader,
                            Block* dedicated_exit,
                            const std::vector<Call*>& calls,
                            const std::unordered_set<Instruction*>& loads_stores,
                            const analysis::PointerAliasing& alias_analysis) {
  const auto context = pointer->get_context();

  const auto value_type = cast<PointerType>(pointer->get_type())->pointee();
  const auto stackalloc = new StackAlloc(context, value_type);
  preheader->push_instruction_front(stackalloc);

  const auto load_pointer_to_stackalloc_after = [&](Instruction* where) {
    const auto load = new Load(context, pointer);
    const auto store = new Store(context, stackalloc, load);
    load->insert_after(where);
    store->insert_after(load);
  };

  const auto store_stackalloc_to_pointer_before = [&](Instruction* where) {
    const auto load = new Load(context, stackalloc);
    const auto store = new Store(context, pointer, load);
    load->insert_before(where);
    store->insert_after(load);
  };

  bool has_stores = false;

  // Rewrite all loads/stores to use stackalloc.
  for (Instruction* user : loads_stores) {
    if (const auto load = cast<Load>(user)) {
      load->set_ptr(stackalloc);
    }

    if (const auto store = cast<Store>(user)) {
      store->set_ptr(stackalloc);
      has_stores = true;
    }
  }

  load_pointer_to_stackalloc_after(stackalloc);
  if (has_stores) {
    store_stackalloc_to_pointer_before(dedicated_exit->get_first_instruction());
  }

  // Insert additional loads and stores around aliasing calls.
  for (Call* call : calls) {
    if (alias_analysis.can_instruction_access_pointer(
          call, pointer, analysis::PointerAliasing::AccessType::All) != analysis::Aliasing::Never) {
      if (has_stores) {
        store_stackalloc_to_pointer_before(call);
      }
      load_pointer_to_stackalloc_after(call);
    }
  }
}

static bool optimize_loop(Function* function,
                          const analysis::Loop* loop,
                          const analysis::PointerAliasing& alias_analysis,
                          DominatorTree& dominator_tree,
                          MemoryDfsContext& dfs_context) {
  // TODO: Check correctness of single exit target. Previous runs may have invalidated it (?).
  const auto exit_target = loop->get_single_exit_target();
  if (!exit_target) {
    return false;
  }

  // Map from pointer to all loads and stores that access it.
  std::unordered_map<Value*, std::unordered_set<Instruction*>> pointers_map;

  // All calls in the loop.
  std::vector<Call*> calls;

  // Get list of all pointers that can be extracted and list of all calls.
  for (const auto block : loop->get_blocks()) {
    for (Instruction& instruction : *block) {
      if (const auto call = cast<Call>(instruction)) {
        calls.push_back(call);
        continue;
      }

      const auto pointer = get_load_store_pointer(&instruction);
      if (!pointer) {
        continue;
      }

      {
        const auto stackalloc = cast<StackAlloc>(pointer);
        if (stackalloc && stackalloc->get_size() == 1) {
          continue;
        }
      }

      // If a pointer is an instruction then it must dominate this loop. Otherwise we cannot extract
      // it.
      if (const auto creation_instruction = cast<Instruction>(pointer)) {
        const auto creation_block = creation_instruction->get_block();
        if (creation_block == loop->get_header() ||
            !creation_block->dominates(loop->get_header(), dominator_tree)) {
          continue;
        }
      }

      pointers_map.insert({pointer, {}});
    }
  }

  {
    std::unordered_set<Value*> invalid_pointers;

    for (const auto block : loop->get_blocks()) {
      for (Instruction& instruction : *block) {
        const auto accessed_pointer = get_load_store_pointer(&instruction);
        if (!accessed_pointer) {
          continue;
        }

        // Go through every known pointer and check the aliasing.
        for (auto& [pointer, accessing_instructions] : pointers_map) {
          const auto alias_result =
            alias_analysis.can_alias(&instruction, pointer, accessed_pointer);

          if (alias_result == analysis::Aliasing::Always) {
            accessing_instructions.insert(&instruction);
          } else if (alias_result == analysis::Aliasing::May) {
            // Both pointers cannot be extracted if aliasing isn't 100% known.
            invalid_pointers.insert(pointer);
            if (pointer != accessed_pointer) {
              invalid_pointers.insert(accessed_pointer);
            }
          }
        }

        // Remove invalid pointers.
        for (const auto invalid_pointer : invalid_pointers) {
          pointers_map.erase(invalid_pointer);
        }

        invalid_pointers.clear();
      }
    }
  }

  // Remove all pointers that are accessed conditionally in the loop. Extracting them would change
  // the program behaviour.
  std::erase_if(pointers_map, [&](const auto& data) {
    return !is_memory_access_unconditional(dfs_context, loop, data.second);
  });

  // No pointers to extract.
  if (pointers_map.empty()) {
    return false;
  }

  // Convert pointer map to the vector and sort it so pointers with most users are in the front.
  std::vector<std::pair<Value*, std::unordered_set<Instruction*>>> pointers;
  {
    pointers.reserve(pointers_map.size());

    std::transform(
      std::move_iterator(pointers_map.begin()), std::move_iterator(pointers_map.end()),
      std::back_inserter(pointers),
      [](std::pair<Value*, std::unordered_set<Instruction*>>&& entry) { return std::move(entry); });

    // Sort in descending order.
    std::sort(pointers.begin(), pointers.end(),
              [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });
  }

  // Create preheader and dedicated exit blocks as they are required to rewrite uses of pointer.
  const auto preheader = utils::get_or_create_loop_preheader(function, loop);
  const auto dedicated_exit = utils::get_or_create_loop_dedicated_exit(function, loop);

  // Update dominator tree as we have affected the control flow.
  dominator_tree = DominatorTree(function);

  std::vector<Value*> rewritten;
  for (const auto& [pointer, data] : pointers) {
    // Make sure that the pointer we are trying to rewrite cannot alias previously rewritten
    // pointers.
    {
      const auto pointer_2 = pointer;
      const bool can_alias = any_of(rewritten, [&](Value* value) {
        return alias_analysis.can_alias(nullptr, pointer_2, value) != analysis::Aliasing::Never;
      });
      if (can_alias) {
        continue;
      }
    }

    rewrite_pointer(pointer, preheader, dedicated_exit, calls, data, alias_analysis);

    rewritten.push_back(pointer);
  }

  return true;
}

static bool optimize_loop_or_sub_loops(Function* function,
                                       const analysis::Loop* loop,
                                       const analysis::PointerAliasing& alias_analysis,
                                       DominatorTree& dominator_tree,
                                       MemoryDfsContext& dfs_context) {
  // Try to optimize this loop.
  if (optimize_loop(function, loop, alias_analysis, dominator_tree, dfs_context)) {
    return true;
  }

  //  // If it didn't work then try optimizing sub-loops.
  bool optimized_subloop = false;

  for (const auto& sub_loop : loop->get_sub_loops()) {
    optimized_subloop |= optimize_loop_or_sub_loops(function, sub_loop.get(), alias_analysis,
                                                    dominator_tree, dfs_context);
  }

  return optimized_subloop;
}

bool opt::LoopMemoryExtraction::run(Function* function) {
  const analysis::PointerAliasing alias_analysis(function);

  DominatorTree dominator_tree(function);
  MemoryDfsContext dfs_context;

  const auto loops = analysis::analyze_function_loops(function);

  bool did_something = false;

  for (const auto& loop : loops) {
    did_something |=
      optimize_loop_or_sub_loops(function, loop.get(), alias_analysis, dominator_tree, dfs_context);
  }

  return did_something;
}