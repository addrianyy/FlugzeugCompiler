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

size_t PathValidator::CacheKeyHash::operator()(const PathValidator::CacheKey& p) const {
  return hash_combine(p.start, p.end, p.memory_kill_target);
}

bool PathValidator::get_blocks_to_check(const std::unordered_set<Block*>*& blocks_to_check,
                                        const DominatorTree& dominator_tree, Instruction* start,
                                        Instruction* end, MemoryKillTarget kill_target) {
  const auto start_block = start->get_block();
  const auto end_block = end->get_block();

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
  get_blocks_from_dominator_to_target(start_block, end_block, blocks);

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
    get_blocks_inbetween(killee, killee, killer, blocks);
  }

  blocks_to_check = &cache.insert({cache_key, std::move(blocks)}).first->second;
  return true;
}

std::optional<size_t> PathValidator::validate_path(const DominatorTree& dominator_tree,
                                                   Instruction* start, Instruction* end,
                                                   const VerifierFn& verifier) {
  return validate_path(dominator_tree, start, end, MemoryKillTarget::None, verifier);
}

std::optional<size_t> PathValidator::validate_path(const DominatorTree& dominator_tree,
                                                   Instruction* start, Instruction* end,
                                                   MemoryKillTarget kill_target,
                                                   const VerifierFn& verifier) {
  size_t instruction_count = 0;

  // Base case: both path points are in the same block.
  if (start->get_block() == end->get_block()) {
    // Start must be before end to make a valid path.
    if (!start->is_before(end)) {
      return std::nullopt;
    }

    // Verify all instructions between `start` and `end`.
    for (Instruction& instruction : instruction_range(start->get_next(), end)) {
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
    if (block == start->get_block()) {
      included_start = true;
    }
    if (block == end->get_block()) {
      included_end = true;
    }

    for (Instruction& instruction : *block) {
      if (!verifier(&instruction)) {
        return std::nullopt;
      }
    }

    instruction_count += block->get_instruction_count();
  }

  // Check start block partially if we haven't checked it previously.
  if (!included_start) {
    // Make sure there is no invalid instruction in the remaining part of start block.
    for (Instruction& instruction : instruction_range(start->get_next(), nullptr)) {
      if (!verifier(&instruction)) {
        return std::nullopt;
      }

      instruction_count++;
    }
  }

  // Check end block partially if we haven't checked it previously.
  if (!included_end) {
    const auto first_instruction = end->get_block()->get_first_instruction();

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
