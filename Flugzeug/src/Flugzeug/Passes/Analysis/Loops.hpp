#pragma once
#include <Flugzeug/IR/Block.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace flugzeug {

class DominatorTree;

/// Properties of the loop:
///   1. All blocks of the loop are strongly connected.
///   2. Loop can be entered only via header.
///   3. All back edges in the loop (but not in sub-loops) must point to loop header.
///   4. All sub-loops can be entered only from this loop and can exit only into this loop.
class Loop {
public:
  Block* header = nullptr;
  std::unordered_set<Block*> blocks;
  std::unordered_set<Block*> blocks_without_subloops;

  std::unordered_set<Block*> back_edges_from;
  std::vector<std::pair<Block*, Block*>> exiting_edges;

  std::vector<std::unique_ptr<Loop>> sub_loops;
};

std::vector<std::unique_ptr<Loop>> analyze_function_loops(Function* function,
                                                          const DominatorTree& dominator_tree);
std::vector<std::unique_ptr<Loop>> analyze_function_loops(Function* function);

} // namespace flugzeug