#include "DeadBlockElimination.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

bool DeadBlockElimination::run(Function* function) {
  // Do DFS traversal on the CFG to find blocks reachable from entry block.
  const auto reachable_blocks =
    function->get_entry_block()->get_reachable_blocks_set(IncludeStart::Yes);

  const bool dead_blocks_found = reachable_blocks.size() != function->get_block_count();
  if (dead_blocks_found) {
    for (Block& block : dont_invalidate_current(*function)) {
      if (reachable_blocks.contains(&block)) {
        continue;
      }

      // Remove all instructions from this block.
      block.clear();

      // Remove all users of the instruction. We need to watch out for iterator invalidation
      // in users list.
      while (block.is_used()) {
        User& user = *block.get_users().begin();

        if (const auto phi = cast<Phi>(user)) {
          // Phis would be handled by block.destroy() anyway but we need to do this here
          // because of iterator invalidation...
          phi->remove_incoming(&block);
        } else if (cast<Branch>(user) || cast<CondBranch>(user)) {
          // These branches are in dead blocks so we can remove them.
          cast<Instruction>(user)->destroy();
        } else {
          unreachable();
        }
      }

      block.destroy();
    }

    // We may have ended up with trivially optimizable Phis. Handle them here to save time.
    for (Block& block : *function) {
      if (block.is_entry_block()) {
        continue;
      }

      for (Instruction& instruction : dont_invalidate_current(block)) {
        if (const auto phi = cast<Phi>(instruction)) {
          utils::simplify_phi(phi, true);
        }
      }
    }
  }

  return dead_blocks_found;
}