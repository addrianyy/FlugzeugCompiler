#include "DeadBlockElimination.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static void destroy_dead_block(Block* block) {
  // Remove all instructions from this block.
  block->clear();

  // Remove branches to this block (they are in dead blocks anyway).
  for (Instruction& instruction : advance_early(block->users<Instruction>())) {
    if (instruction.is_branching()) {
      // These branches are in dead blocks, so we can remove them.
      instruction.destroy();
    }
  }

  block->destroy();
}

bool opt::DeadBlockElimination::run(Function* function) {
  // Do DFS traversal on the CFG to find blocks reachable from entry block.
  const auto reachable_blocks =
    function->get_entry_block()->get_reachable_blocks_set(IncludeStart::Yes);

  const bool dead_blocks_found = reachable_blocks.size() != function->get_block_count();
  if (dead_blocks_found) {
    for (Block& block : advance_early(*function)) {
      if (!reachable_blocks.contains(&block)) {
        destroy_dead_block(&block);
      }
    }

    // We may have ended up with trivially optimizable Phis. Handle them here to save time.
    for (Block& block : *function) {
      if (block.is_entry_block()) {
        continue;
      }

      for (Phi& phi : advance_early(block.instructions<Phi>())) {
        utils::simplify_phi(&phi, true);
      }
    }
  }

  return dead_blocks_found;
}
