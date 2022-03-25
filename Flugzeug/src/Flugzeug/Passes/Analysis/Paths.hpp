#pragma once
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <Flugzeug/IR/Block.hpp>

namespace flugzeug::analysis {

namespace detail {

class BlockChildren {
  BlockTargets<Block> successors;
  size_t index = 0;

public:
  BlockChildren(Block* block, Block* barrier) {
    for (const auto successor : block->successors()) {
      if (successor != barrier) {
        successors.push_back(successor);
      }
    }
  }

  std::optional<Block*> next() {
    if (index >= successors.size()) {
      return std::nullopt;
    }

    return successors[index++];
  }
};

} // namespace detail

class PathAnalysisWorkData {
  friend void get_blocks_inbetween(Block* from, Block* to, Block* barrier,
                                   std::unordered_set<Block*>& blocks_inbetween,
                                   PathAnalysisWorkData* work_data);
  friend void get_blocks_from_dominator_to_target(Block* dominator, Block* target,
                                                  std::unordered_set<Block*>& blocks_inbetween,
                                                  PathAnalysisWorkData* work_data);

  std::unordered_set<Block*> visited;
  std::vector<Block*> blocks;
  std::vector<detail::BlockChildren> children;
};

void get_blocks_inbetween(Block* from, Block* to, Block* barrier,
                          std::unordered_set<Block*>& blocks_inbetween,
                          PathAnalysisWorkData* work_data = nullptr);
std::unordered_set<Block*> get_blocks_inbetween(Block* from, Block* to, Block* barrier,
                                                PathAnalysisWorkData* work_data = nullptr);

void get_blocks_from_dominator_to_target(Block* dominator, Block* target,
                                         std::unordered_set<Block*>& blocks_inbetween,
                                         PathAnalysisWorkData* work_data = nullptr);
std::unordered_set<Block*>
get_blocks_from_dominator_to_target(Block* dominator, Block* target,
                                    PathAnalysisWorkData* work_data = nullptr);

class PathValidator {
public:
  enum class MemoryKillTarget {
    Start,
    End,
    None,
  };

private:
  struct CacheKey {
    Block* start;
    Block* end;
    MemoryKillTarget memory_kill_target;

    bool operator==(const CacheKey& other) const {
      return start == other.start && end == other.end &&
             memory_kill_target == other.memory_kill_target;
    }
  };

  struct CacheKeyHash {
    size_t operator()(const CacheKey& p) const;
  };

  PathAnalysisWorkData work_data;

  std::unordered_map<CacheKey, std::unordered_set<Block*>, CacheKeyHash> cache;

  bool get_blocks_to_check(const std::unordered_set<Block*>*& blocks_to_check,
                           const DominatorTree& dominator_tree, Instruction* start,
                           Instruction* end, MemoryKillTarget kill_target);

public:
  using VerifierFn = std::function<bool(const Instruction*)>;

  std::optional<size_t> validate_path(const DominatorTree& dominator_tree, Instruction* start,
                                      Instruction* end, const VerifierFn& verifier);

  std::optional<size_t> validate_path(const DominatorTree& dominator_tree, Instruction* start,
                                      Instruction* end, MemoryKillTarget kill_target,
                                      const VerifierFn& verifier);

  std::optional<size_t> validate_path_count(const DominatorTree& dominator_tree, Instruction* start,
                                            Instruction* end);
};

} // namespace flugzeug::analysis