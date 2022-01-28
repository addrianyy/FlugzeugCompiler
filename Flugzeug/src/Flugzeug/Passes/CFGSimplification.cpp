#include "CFGSimplification.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

Block* CFGSimplification::get_intermediate_block_target(Block* block) {
  if (block->get_instruction_count() != 1) {
    return nullptr;
  }

  if (const auto branch = cast<Branch>(block->get_last_instruction())) {
    const auto target = branch->get_target();

    // Skip infinite loops.
    if (target != block) {
      return target;
    }
  }

  return nullptr;
}

bool CFGSimplification::do_phis_depend_on_predecessors(const Block* block, const Block* p1,
                                                       const Block* p2) {
  verify(p1 != p2 && block != p1 && block != p2, "Invalid blocks");

  for (const Phi& phi : block->instructions<Phi>()) {
    const auto p1_value = phi.get_incoming_by_block(p1);
    const auto p2_value = phi.get_incoming_by_block(p2);

    if ((p1_value && p2_value) && p1_value != p2_value) {
      return true;
    }
  }

  return false;
}

Block* CFGSimplification::thread_jump(Block* block, Block* target, bool* did_something) {
  if (block == target) {
    return nullptr;
  }

  if (const auto actual_target = get_intermediate_block_target(target)) {
    if (!do_phis_depend_on_predecessors(actual_target, block, target)) {
      actual_target->replace_incoming_blocks_in_phis(target, block);

      *did_something = true;

      return actual_target;
    }
  }

  return nullptr;
}

bool CFGSimplification::thread_jumps(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    // Skip intermediate blocks.
    if (get_intermediate_block_target(&block)) {
      continue;
    }

    const auto last_instruction = block.get_last_instruction();

    if (const auto branch = cast<Branch>(last_instruction)) {
      if (const auto new_target = thread_jump(&block, branch->get_target(), &did_something)) {
        branch->set_target(new_target);
      }
    } else if (const auto cond_branch = cast<CondBranch>(last_instruction)) {
      if (const auto new_target =
            thread_jump(&block, cond_branch->get_true_target(), &did_something)) {
        cond_branch->set_true_target(new_target);
      }

      if (const auto new_target =
            thread_jump(&block, cond_branch->get_false_target(), &did_something)) {
        cond_branch->set_false_target(new_target);
      }
    }
  }

  return did_something;
}

bool CFGSimplification::merge_blocks(Function* function) {
  bool did_something = false;

  for (Block& block : dont_invalidate_current(*function)) {
    const auto predecessor = block.get_single_predecessor();
    if (block.is_entry_block() || !predecessor || predecessor == &block) {
      continue;
    }

    const auto branch_to_block = cast<Branch>(predecessor->get_last_instruction());
    if (!branch_to_block) {
      continue;
    }

    // Move all instructions from `block` to `predecessor`.
    while (!block.is_empty()) {
      const auto instruction = block.get_first_instruction();
      instruction->move_before(branch_to_block);

      if (const auto phi = cast<Phi>(instruction)) {
        // Phi should have either 0 or 1 incoming values. It must be simplificable.
        verify(utils::simplify_phi(phi, false), "Failed to simplify Phi");
      }
    }

    // Destroy branch to `block`. `predecessor` will now branch to `block` successors.
    branch_to_block->destroy();

    for (Block* successor : predecessor->get_successors()) {
      successor->replace_incoming_blocks_in_phis(&block, predecessor);
    }

    block.destroy();

    did_something = true;
  }

  return did_something;
}

bool CFGSimplification::run(Function* function) {
  return thread_jumps(function) | merge_blocks(function);
}