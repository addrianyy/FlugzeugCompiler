#include "DominatorTree.hpp"
#include "Block.hpp"
#include "Function.hpp"

using namespace flugzeug;

static std::vector<const Block*> traverse_dfs_postorder(const Block* entry_block) {
  const size_t block_count = entry_block->function()->block_count();

  std::vector<const Block*> result;
  std::vector<const Block*> stack;
  std::unordered_set<const Block*> visited;
  std::unordered_set<const Block*> finished;

  result.reserve(block_count);
  stack.reserve(std::min(size_t(8), block_count));
  visited.reserve(block_count);
  finished.reserve(block_count);

  stack.push_back(entry_block);

  while (!stack.empty()) {
    const auto block = stack.back();

    if (visited.insert(block).second) {
      for (const Block* successor : block->successors()) {
        if (!visited.contains(successor)) {
          stack.push_back(successor);
        }
      }
    } else {
      stack.pop_back();

      if (finished.insert(block).second) {
        result.push_back(block);
      }
    }
  }

  return result;
}

static size_t intersect(const std::vector<size_t>& dominators, size_t finger1, size_t finger2) {
  while (true) {
    if (finger1 < finger2) {
      finger1 = dominators[finger1];
    } else if (finger1 > finger2) {
      finger2 = dominators[finger2];
    } else {
      return finger1;
    }
  }
}

static std::unordered_map<const Block*, const Block*> calculate_immediate_dominators(
  const Block* entry_block) {
  const auto postorder = traverse_dfs_postorder(entry_block);
  const auto entry_index = postorder.size() - 1;

  verify(!postorder.empty(), "Postorder travarsal returned no blocks");
  verify(postorder[entry_index] == entry_block, "Invalid postorder traversal results");

  // If we have only entry block then dominance map will be empty.
  if (postorder.size() <= 1) {
    return {};
  }

  std::vector<std::vector<size_t>> predecessors_map;
  predecessors_map.reserve(postorder.size());

  {
    std::unordered_map<const Block*, size_t> block_to_index;
    block_to_index.reserve(postorder.size());

    for (size_t i = 0; i < postorder.size(); ++i) {
      block_to_index[postorder[i]] = i;
    }

    for (const Block* block : postorder) {
      const auto predecessors = block->predecessors();

      std::vector<size_t> predecessor_indices;
      predecessor_indices.reserve(predecessors.size());

      for (const Block* predecessor : predecessors) {
        const auto it = block_to_index.find(predecessor);
        if (it == block_to_index.end()) {
          // This can happen for dead blocks.
          continue;
        }

        predecessor_indices.push_back(it->second);
      }

      predecessors_map.push_back(std::move(predecessor_indices));
    }
  }

  constexpr static size_t undefined = std::numeric_limits<size_t>::max();

  std::vector<size_t> dominators(postorder.size(), undefined);

  {
    dominators[entry_index] = entry_index;

    bool changed = true;

    while (changed) {
      changed = false;

      // Itarate over [0, length - 1) in reverse.
      // TODO: Use ranges when they work with Clion.
      // for (size_t index : std::views::iota(size_t(0), size - 1) | std::views::reverse)
      for (size_t i = 0; i < postorder.size() - 1; ++i) {
        const size_t index = postorder.size() - 2 - i;

        verify(postorder[index] != entry_block, "Unexpected entry block");

        size_t new_idom_index = undefined;
        for (const size_t predecessor : predecessors_map[index]) {
          if (dominators[predecessor] == undefined) {
            continue;
          }

          if (new_idom_index == undefined) {
            new_idom_index = predecessor;
          } else {
            new_idom_index = intersect(dominators, new_idom_index, predecessor);
          }
        }

        verify(new_idom_index < postorder.size(), "Calculating idom index failed");

        if (new_idom_index != dominators[index]) {
          dominators[index] = new_idom_index;
          changed = true;
        }
      }
    }
  }

  std::unordered_map<const Block*, const Block*> final_dominators;
  final_dominators.reserve(dominators.size());

  for (size_t i = 0; i < postorder.size(); ++i) {
    const auto dominator = dominators[i];
    verify(dominator != undefined, "Not every dominator was calculated");

    if (i == entry_index) {
      continue;
    }

    final_dominators[postorder[i]] = postorder[dominator];
  }

  return final_dominators;
}

bool DominatorTree::first_dominates_second(const Block* dominator, const Block* block) const {
  if (dominator == block) {
    return true;
  }

  verify(dominator->function() == block->function(),
         "`first_dominates_second` works only on blocks that belong to the same function");

  if (dominator->is_entry_block()) {
    return true;
  }

  while (block) {
    if (block == dominator) {
      return true;
    }

    block = immediate_dominator(block);
  }

  return false;
}

DominatorTree::DominatorTree(const Function* function)
    : immediate_dominators(calculate_immediate_dominators(function->entry_block())) {
  verify(!immediate_dominator(function->entry_block()),
         "Entry block shouldn't have immediate dominator");
}

bool DominatorTree::is_block_dead(const Block* block) const {
  return !block->is_entry_block() && !immediate_dominators.contains(block);
}

const Block* DominatorTree::immediate_dominator(const Block* block) const {
  const auto it = immediate_dominators.find(block);
  if (it == immediate_dominators.end()) {
    return nullptr;
  }

  return it->second;
}