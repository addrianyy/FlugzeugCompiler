#include "Paths.hpp"

using namespace flugzeug;
using namespace flugzeug::analysis;

static void process_block(Block* current, Block* destination, Block* barrier,
                          std::unordered_set<Block*>& visited, std::vector<Block*>& path,
                          std::unordered_set<Block*>& blocks_inbetween, bool first_iteration) {
  const bool ignore_block = first_iteration && current == destination;

  if (!ignore_block) {
    visited.insert(current);
  }
  path.push_back(current);

  if (!ignore_block && current == destination) {
    for (const auto block_in_path : path) {
      blocks_inbetween.insert(block_in_path);
    }
  } else {
    for (const auto target : current->successors()) {
      if (target == barrier || visited.contains(target)) {
        continue;
      }

      process_block(target, destination, barrier, visited, path, blocks_inbetween, false);
    }
  }

  path.pop_back();
  if (!ignore_block) {
    visited.erase(current);
  }
}

void analysis::get_blocks_inbetween(Block* from, Block* to, Block* barrier,
                                    std::unordered_set<Block*>& blocks_inbetween) {
  verify(from != barrier && to != barrier, "Invalid barrier block");

  std::unordered_set<Block*> visited;
  std::vector<Block*> path;

  process_block(from, to, barrier, visited, path, blocks_inbetween, true);
}

std::unordered_set<Block*> analysis::get_blocks_inbetween(Block* from, Block* to, Block* barrier) {
  std::unordered_set<Block*> blocks_inbetween;
  get_blocks_inbetween(from, to, barrier, blocks_inbetween);
  return blocks_inbetween;
}

void analysis::get_blocks_from_dominator_to_target(Block* dominator, Block* target,
                                                   std::unordered_set<Block*>& blocks_inbetween) {
  std::unordered_set<Block*> visited;
  std::vector<Block*> stack;

  // Start traversing from the end of the path and go upwards.
  stack.push_back(target);

  while (!stack.empty()) {
    const auto block = stack.back();
    stack.pop_back();

    if (!visited.insert(block).second) {
      continue;
    }

    for (const auto predecessor : block->predecessors()) {
      // Because `dominator` dominates `target` block we must eventually hit `dominator` block
      // during traversal. In this case we shouldn't go up.
      if (predecessor == dominator || visited.contains(predecessor)) {
        continue;
      }

      stack.push_back(predecessor);
    }
  }

  blocks_inbetween.reserve(blocks_inbetween.size() + visited.size() + 1);

  for (Block* block : visited) {
    blocks_inbetween.insert(block);
  }

  // Insert `dominator` too as it was skipped during traversal.
  blocks_inbetween.insert(dominator);
}

std::unordered_set<Block*> analysis::get_blocks_from_dominator_to_target(Block* dominator,
                                                                         Block* target) {
  std::unordered_set<Block*> blocks_inbetween;
  get_blocks_from_dominator_to_target(dominator, target, blocks_inbetween);
  return blocks_inbetween;
}