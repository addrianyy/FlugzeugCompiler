#include "MemoryToSSA.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/Core/Iterator.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static bool is_stackalloc_optimizable(const StackAlloc* stackalloc) {
  // We cannot optimize out arrays.
  if (stackalloc->get_size() != 1) {
    return false;
  }

  for (const User& user : stackalloc->users()) {
    // Stackalloc is optimizable if it's only used by Load and Store instructions.
    const auto store = cast<Store>(user);
    const auto load = cast<Load>(user);

    const auto valid_use = load || (store && store->get_ptr() == stackalloc);
    if (!valid_use) {
      return false;
    }
  }

  return true;
}

static std::vector<StackAlloc*> find_optimizable_stackallocs(Function* function) {
  std::vector<StackAlloc*> stackallocs;

  for (StackAlloc& stackalloc : function->instructions<StackAlloc>()) {
    if (is_stackalloc_optimizable(&stackalloc)) {
      stackallocs.push_back(&stackalloc);
    }
  }

  return stackallocs;
}

static Value* get_value_for_first_use(Block* block,
                                      StackAlloc* stackalloc,
                                      std::vector<Phi*>& inserted_phis) {
  const auto type = stackalloc->get_allocated_type();

  if (block->is_entry_block()) {
    return type->undef();
  } else {
    auto phi = new Phi(block->context(), type);

    block->push_instruction_front(phi);
    inserted_phis.push_back(phi);

    return phi;
  }
}

static void optimize_stackalloc(StackAlloc* stackalloc) {
  std::unordered_map<Block*, Value*> block_values;
  std::vector<Phi*> inserted_phis;

  const auto reachable = stackalloc->block()->reachable_blocks_set(IncludeStart::Yes);

  for (Block* block : reachable) {
    Value* current_value = nullptr;

    for (Instruction& instruction : advance_early(*block)) {
      if (const auto load = cast<Load>(instruction)) {
        if (load->get_ptr() == stackalloc) {
          // Pointer wasn't written to in this block. We will need to take value from PHI
          // instruction (or make it undef for entry block).
          if (!current_value) {
            current_value = get_value_for_first_use(block, stackalloc, inserted_phis);
          }

          // This load will use currently known value.
          load->replace_uses_with_and_destroy(current_value);
        }
      } else if (const auto store = cast<Store>(instruction)) {
        if (store->get_ptr() == stackalloc) {
          // Source next loads of pointer from stored value.
          current_value = store->get_val();
          store->destroy();
        }
      }
    }

    // Even if we didn't use pointer in this block it may be needed in our successors.
    if (!current_value) {
      current_value = get_value_for_first_use(block, stackalloc, inserted_phis);
    }

    block_values.insert(std::pair{block, current_value});
  }

  // Go through all inserted Phis and add proper incoming values.
  for (Phi* phi : inserted_phis) {
    Block* block = phi->block();

    for (Block* pred : block->predecessors()) {
      const auto it = block_values.find(pred);
      const auto value =
        it != block_values.end() ? it->second : stackalloc->get_allocated_type()->undef();

      phi->add_incoming(pred, value);
    }
  }

  // Remove unused Phis and optimize Phis with zero or one incoming values.
  for (Phi* phi : inserted_phis) {
    utils::simplify_phi(phi, true);
  }

  stackalloc->destroy();
}

bool opt::MemoryToSSA::run(Function* function) {
  const auto optimizable = find_optimizable_stackallocs(function);
  for (StackAlloc* stackalloc : optimizable) {
    optimize_stackalloc(stackalloc);
  }

  return !optimizable.empty();
}