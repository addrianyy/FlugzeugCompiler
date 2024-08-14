#include "Paths.hpp"

using namespace flugzeug;
using namespace flugzeug::analysis;

static bool can_reach(Block* from,
                      Block* to,
                      Block* barrier,
                      std::vector<Block*>& stack,
                      std::unordered_set<Block*>& visited) {
  stack.push_back(from);

  while (!stack.empty()) {
    const auto block = stack.back();
    stack.pop_back();

    if (!visited.insert(block).second) {
      continue;
    }

    for (const auto successor : block->successors()) {
      if (successor == to) {
        return true;
      }

      if (successor == barrier || visited.contains(successor)) {
        continue;
      }

      stack.push_back(successor);
    }
  }

  return false;
}

void analysis::get_blocks_inbetween(Block* from,
                                    Block* to,
                                    Block* barrier,
                                    std::unordered_set<Block*>& blocks_inbetween,
                                    PathAnalysisWorkData* work_data) {
  verify(from != barrier && to != barrier, "Invalid barrier block");

  std::unordered_set<Block*> buffer_visited;
  std::vector<Block*> buffer_path;
  std::vector<detail::BlockChildren> buffer_stack;

  std::unordered_set<Block*>* visited;
  std::vector<Block*>* path;
  std::vector<detail::BlockChildren>* stack;

  if (work_data) {
    visited = &work_data->visited;
    path = &work_data->blocks;
    stack = &work_data->children;

    visited->clear();
    path->clear();
    stack->clear();
  } else {
    visited = &buffer_visited;
    path = &buffer_path;
    stack = &buffer_stack;
  }

  // Fast case: if we cannot reach `to` from `from` we don't need to do anything.
  if (!can_reach(from, to, barrier, *path, *visited)) {
    return;
  }

  visited->clear();
  path->clear();

  visited->insert(from);
  path->emplace_back(from);
  stack->emplace_back(from, barrier);

  while (!stack->empty()) {
    auto& children = stack->back();

    if (const auto child = children.next()) {
      if (*child == to) {
        for (const auto path_block : *path) {
          blocks_inbetween.insert(path_block);
        }

        blocks_inbetween.insert(to);
      } else if (!visited->contains(*child)) {
        visited->insert(*child);
        path->emplace_back(*child);
        stack->emplace_back(*child, barrier);
      }
    } else {
      visited->erase(path->back());
      path->pop_back();
      stack->pop_back();
    }
  }
}

std::unordered_set<Block*> analysis::get_blocks_inbetween(Block* from,
                                                          Block* to,
                                                          Block* barrier,
                                                          PathAnalysisWorkData* work_data) {
  std::unordered_set<Block*> blocks_inbetween;
  get_blocks_inbetween(from, to, barrier, blocks_inbetween, work_data);
  return blocks_inbetween;
}

void analysis::get_blocks_from_dominator_to_target(Block* dominator,
                                                   Block* target,
                                                   std::unordered_set<Block*>& blocks_inbetween,
                                                   PathAnalysisWorkData* work_data) {
  std::unordered_set<Block*> buffer_visited;
  std::vector<Block*> buffer_stack;

  std::unordered_set<Block*>* visited;
  std::vector<Block*>* stack;

  if (work_data) {
    visited = &work_data->visited;
    stack = &work_data->blocks;

    visited->clear();
    stack->clear();
  } else {
    visited = &buffer_visited;
    stack = &buffer_stack;
  }

  // Start traversing from the end of the path and go upwards.
  stack->push_back(target);

  while (!stack->empty()) {
    const auto block = stack->back();
    stack->pop_back();

    if (!visited->insert(block).second) {
      continue;
    }

    for (const auto predecessor : block->predecessors()) {
      // Because `dominator` dominates `target` block we must eventually hit `dominator` block
      // during traversal. In this case we shouldn't go up.
      if (predecessor == dominator || visited->contains(predecessor)) {
        continue;
      }

      stack->push_back(predecessor);
    }
  }

  blocks_inbetween.reserve(blocks_inbetween.size() + visited->size() + 1);

  for (Block* block : *visited) {
    blocks_inbetween.insert(block);
  }

  // Insert `dominator` too as it was skipped during traversal.
  blocks_inbetween.insert(dominator);
}

std::unordered_set<Block*> analysis::get_blocks_from_dominator_to_target(
  Block* dominator,
  Block* target,
  PathAnalysisWorkData* work_data) {
  std::unordered_set<Block*> blocks_inbetween;
  get_blocks_from_dominator_to_target(dominator, target, blocks_inbetween, work_data);
  return blocks_inbetween;
}

size_t PathValidator::CacheKeyHash::operator()(const PathValidator::CacheKey& p) const {
  return combine_hash(p.start, p.end, p.memory_kill_target);
}

bool PathValidator::get_blocks_to_check(const std::unordered_set<Block*>*& blocks_to_check,
                                        const DominatorTree& dominator_tree,
                                        Instruction* start,
                                        Instruction* end,
                                        MemoryKillTarget kill_target) {
  const auto start_block = start->block();
  const auto end_block = end->block();

  const CacheKey cache_key{start_block, end_block, kill_target};

  // First try to lookup blocks to check in the cache.
  if (const auto it = cache.find(cache_key); it != cache.end()) {
    blocks_to_check = &it->second;
    return true;
  }

  // When path points are in different blocks then start block must dominate end block.
  if (!start_block->dominates(end_block, dominator_tree)) {
    return false;
  }

  std::unordered_set<Block*> blocks;
  get_blocks_from_dominator_to_target(start_block, end_block, blocks, &work_data);

  // Remove `start` and `end` blocks as they will be only checked partially.
  blocks.erase(start_block);
  blocks.erase(end_block);

  if (kill_target != MemoryKillTarget::None) {
    Block* killer = start_block;
    Block* killee = end_block;

    if (kill_target == MemoryKillTarget::Start) {
      std::swap(killer, killee);
    }

    // Get all blocks that take part in a cycle that allows `killee` to reach itself without hitting
    // `killer` (if such cycle exists).
    get_blocks_inbetween(killee, killee, killer, blocks, &work_data);
  }

  blocks_to_check = &cache.insert({cache_key, std::move(blocks)}).first->second;
  return true;
}

std::optional<size_t> PathValidator::validate_path(const DominatorTree& dominator_tree,
                                                   Instruction* start,
                                                   Instruction* end,
                                                   const VerifierFn& verifier) {
  return validate_path(dominator_tree, start, end, MemoryKillTarget::None, verifier);
}

std::optional<size_t> PathValidator::validate_path(const DominatorTree& dominator_tree,
                                                   Instruction* start,
                                                   Instruction* end,
                                                   MemoryKillTarget kill_target,
                                                   const VerifierFn& verifier) {
  size_t instruction_count = 0;

  // Base case: both path points are in the same block.
  if (start->block() == end->block()) {
    // Start must be before end to make a valid path.
    if (!start->is_before(end)) {
      return std::nullopt;
    }

    // Verify all instructions between `start` and `end`.
    for (Instruction& instruction : instruction_range(start->next(), end)) {
      if (!verifier(&instruction)) {
        return std::nullopt;
      }

      instruction_count++;
    }

    return instruction_count;
  }

  const std::unordered_set<Block*>* blocks_to_check;
  if (!get_blocks_to_check(blocks_to_check, dominator_tree, start, end, kill_target)) {
    return std::nullopt;
  }

  bool included_start = false;
  bool included_end = false;

  // Check instructions in all blocks in the path.
  for (Block* block : *blocks_to_check) {
    if (block == start->block()) {
      included_start = true;
    }
    if (block == end->block()) {
      included_end = true;
    }

    for (Instruction& instruction : *block) {
      if (!verifier(&instruction)) {
        return std::nullopt;
      }
    }

    instruction_count += block->instruction_count();
  }

  // Check start block partially if we haven't checked it previously.
  if (!included_start) {
    // Make sure there is no invalid instruction in the remaining part of start block.
    for (Instruction& instruction : instruction_range(start->next(), nullptr)) {
      if (!verifier(&instruction)) {
        return std::nullopt;
      }

      instruction_count++;
    }
  }

  // Check end block partially if we haven't checked it previously.
  if (!included_end) {
    const auto first_instruction = end->block()->first_instruction();

    // Make sure there is no invalid instruction in the initial part of end block.
    for (Instruction& instruction : instruction_range(first_instruction, end)) {
      if (!verifier(&instruction)) {
        return std::nullopt;
      }

      instruction_count++;
    }
  }

  return instruction_count;
}

std::optional<size_t> PathValidator::validate_path_count(const DominatorTree& dominator_tree,
                                                         Instruction* start,
                                                         Instruction* end) {
  size_t instruction_count = 0;

  // Base case: both path points are in the same block.
  if (start->block() == end->block()) {
    // Start must be before end to make a valid path.
    if (!start->is_before(end)) {
      return std::nullopt;
    }

    // Verify all instructions between `start` and `end`.
    for (Instruction& instruction : instruction_range(start->next(), end)) {
      instruction_count++;
    }

    return instruction_count;
  }

  const std::unordered_set<Block*>* blocks_to_check;
  if (!get_blocks_to_check(blocks_to_check, dominator_tree, start, end, MemoryKillTarget::None)) {
    return std::nullopt;
  }

  for (Block* block : *blocks_to_check) {
    verify(block != start->block() && block != end->block(),
           "Encountered unexpected block");
    instruction_count += block->instruction_count();
  }

  for (Instruction& instruction : instruction_range(start->next(), nullptr)) {
    instruction_count++;
  }

  const auto first_instruction = end->block()->first_instruction();
  for (Instruction& instruction : instruction_range(first_instruction, end)) {
    instruction_count++;
  }

  return instruction_count;
}
