#include "CFGSimplification.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static Block* get_intermediate_block_target(Block* block) {
  if (block->instruction_count() != 1) {
    return nullptr;
  }

  if (const auto branch = cast<Branch>(block->last_instruction())) {
    const auto target = branch->target();

    // Skip infinite loops.
    if (target != block) {
      return target;
    }
  }

  return nullptr;
}

static bool do_phis_depend_on_predecessors(const Block* block, const Block* p1, const Block* p2) {
  verify(p1 != p2 && block != p1 && block != p2, "Invalid blocks");

  for (const Phi& phi : block->instructions<Phi>()) {
    const auto p1_value = phi.incoming_for_block(p1);
    const auto p2_value = phi.incoming_for_block(p2);

    if ((p1_value && p2_value) && p1_value != p2_value) {
      return true;
    }
  }

  return false;
}

static Block* thread_jump(Block* block, Block* target, bool* did_something) {
  if (block == target) {
    return nullptr;
  }

  if (const auto actual_target = get_intermediate_block_target(target)) {
    if (block == actual_target) {
      return nullptr;
    }

    if (!do_phis_depend_on_predecessors(actual_target, block, target)) {
      for (Phi& phi : actual_target->instructions<Phi>()) {
        phi.add_incoming(block, phi.incoming_for_block(target));
      }

      *did_something = true;

      return actual_target;
    }
  }

  return nullptr;
}

static bool thread_jumps(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    // Skip intermediate blocks.
    if (get_intermediate_block_target(&block)) {
      continue;
    }

    const auto last_instruction = block.last_instruction();

    if (const auto branch = cast<Branch>(last_instruction)) {
      if (const auto new_target = thread_jump(&block, branch->target(), &did_something)) {
        branch->set_target(new_target);
      }
    } else if (const auto cond_branch = cast<CondBranch>(last_instruction)) {
      if (const auto new_target =
            thread_jump(&block, cond_branch->true_target(), &did_something)) {
        cond_branch->set_true_target(new_target);
      }

      if (const auto new_target =
            thread_jump(&block, cond_branch->false_target(), &did_something)) {
        cond_branch->set_false_target(new_target);
      }
    }
  }

  // Remove all intermediate blocks that aren't used anymore.
  for (Block& block : advance_early(*function)) {
    if (!block.is_entry_block() && block.predecessors().empty()) {
      block.clear_and_destroy();
    }
  }

  return did_something;
}

static bool merge_blocks(Function* function) {
  bool did_something = false;

  for (Block& block : advance_early(*function)) {
    const auto predecessor = block.single_predecessor();
    if (block.is_entry_block() || !predecessor || predecessor == &block) {
      continue;
    }

    const auto branch_to_block = cast<Branch>(predecessor->last_instruction());
    if (!branch_to_block) {
      continue;
    }

    // Move all instructions from `block` to `predecessor`.
    while (!block.empty()) {
      const auto instruction = block.first_instruction();
      instruction->move_before(branch_to_block);

      if (const auto phi = cast<Phi>(instruction)) {
        // Phi should have either 0 or 1 incoming values. It must be simplificable.
        verify(utils::simplify_phi(phi, false), "Failed to simplify Phi");
      }
    }

    // Destroy branch to `block`. `predecessor` will now branch to `block` successors.
    branch_to_block->destroy();

    for (Block* successor : predecessor->successors()) {
      successor->replace_incoming_blocks_in_phis(&block, predecessor);
    }

    block.destroy();

    did_something = true;
  }

  return did_something;
}

bool opt::CFGSimplification::run(Function* function) {
  return thread_jumps(function) | merge_blocks(function);
}