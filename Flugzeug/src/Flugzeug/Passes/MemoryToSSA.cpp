#include "MemoryToSSA.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/Core/Iterator.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

struct MemoryToSsaContext {
  std::vector<Phi*> inserted_phis;
  std::unordered_map<Block*, Value*> values_at_blocks;
};

static bool is_stackalloc_optimizable(const StackAlloc* stackalloc) {
  // Stackalloc is optimizable if it's scalar and it's only used by Load and Store instructions.

  return stackalloc->is_scalar() && all_of(stackalloc->users(), [&](const User& user) {
           const auto load = cast<Load>(user);
           const auto store = cast<Store>(user);

           return load || (store && store->address() == stackalloc);
         });
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

static Value* get_value_for_first_block_use(Type* type,
                                            Block* block,
                                            std::vector<Phi*>& inserted_phis) {
  if (block->is_entry_block()) {
    return type->undef();
  }

  const auto phi = new Phi(block->context(), type);

  block->push_instruction_front(phi);
  inserted_phis.push_back(phi);

  return phi;
}

static void optimize_stackalloc(MemoryToSsaContext& ctx, StackAlloc* stackalloc) {
  const auto type = stackalloc->allocated_type();
  const auto reachable = stackalloc->block()->reachable_blocks_set(IncludeStart::Yes);

  for (Block* block : reachable) {
    Value* current_value = nullptr;

    for (Instruction& instruction : advance_early(*block)) {
      if (const auto load = cast<Load>(instruction)) {
        if (load->address() == stackalloc) {
          // Address wasn't written to in this block. We will need to take value from Phi
          // instruction (or make it undef for entry block).
          if (!current_value) {
            current_value = get_value_for_first_block_use(type, block, ctx.inserted_phis);
          }

          // This load will use currently known value.
          load->replace_uses_with_and_destroy(current_value);
        }
      } else if (const auto store = cast<Store>(instruction)) {
        if (store->address() == stackalloc) {
          // Source next loads of this address from stored value.
          current_value = store->value();
          store->destroy();
        }
      }
    }

    // Even if we didn't use the address in this block it may be needed in our successors.
    if (!current_value) {
      current_value = get_value_for_first_block_use(type, block, ctx.inserted_phis);
    }

    ctx.values_at_blocks.insert(std::pair{block, current_value});
  }

  // Go through all inserted Phis and add proper incoming values.
  for (Phi* phi : ctx.inserted_phis) {
    Block* block = phi->block();

    for (Block* predecessor : block->predecessors()) {
      const auto it = ctx.values_at_blocks.find(predecessor);
      const auto value = it != ctx.values_at_blocks.end() ? it->second : type->undef();

      phi->add_incoming(predecessor, value);
    }
  }

  // Remove unused Phis and optimize Phis with zero or one incoming values.
  for (Phi* phi : ctx.inserted_phis) {
    utils::simplify_phi(phi, true);
  }

  stackalloc->destroy();
}

bool opt::MemoryToSSA::run(Function* function) {
  const auto optimizable = find_optimizable_stackallocs(function);

  if (!optimizable.empty()) {
    MemoryToSsaContext context{};

    for (StackAlloc* stackalloc : optimizable) {
      context.inserted_phis.clear();
      context.values_at_blocks.clear();

      optimize_stackalloc(context, stackalloc);
    }
  }

  return !optimizable.empty();
}