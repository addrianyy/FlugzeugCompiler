#include "DeadBlockElimination.hpp"
#include <IR/Block.hpp>
#include <IR/Function.hpp>
#include <IR/Instructions.hpp>

using namespace flugzeug;

bool DeadBlockElimination::run(Function* function) {
  std::unordered_set<Block*> reachable_blocks;

  // Do DFS traversal on the CFG to find block reachable from entry block.
  {
    std::vector<Block*> stack;
    stack.push_back(function->get_entry_block());

    while (!stack.empty()) {
      Block* block = stack.back();
      stack.pop_back();

      reachable_blocks.insert(block);

      for (Block* successor : block->get_successors()) {
        if (!reachable_blocks.contains(successor)) {
          stack.push_back(successor);
        }
      }
    }
  }

  const bool dead_blocks_found = reachable_blocks.size() != function->get_block_count();
  if (dead_blocks_found) {
    // Do first pass to remove all instructions from dead blocks. We need to do it before removing
    // blocks to remove all unreachable branches to dead blocks.
    for (Block& block : *function) {
      if (!reachable_blocks.contains(&block)) {
        block.clear();
      }
    }

    for (Block& block : dont_invalidate_current(*function)) {
      if (!reachable_blocks.contains(&block)) {
        block.destroy();
      }
    }

    // We may have ended up with Phis that contain only one incoming value. Optimize them out here
    // to save time.
    for (Block& block : *function) {
      if (block.is_entry_block()) {
        continue;
      }

      for (Instruction& instruction : dont_invalidate_current(block)) {
        if (const auto phi = cast<Phi>(instruction)) {
          if (const auto incoming = phi->get_single_incoming_value()) {
            phi->replace_uses_and_destroy(incoming);
          }
        }
      }
    }
  }

  return dead_blocks_found;
}