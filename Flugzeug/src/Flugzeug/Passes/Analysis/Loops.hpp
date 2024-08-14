#pragma once
#include <Flugzeug/IR/Block.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace flugzeug {

class DominatorTree;

namespace analysis {

template <typename T>
struct SccContext;

/// Properties of the loop:
///   1. All blocks of the loop are strongly connected.
///   2. Loop can be entered only via header.
///   3. All back edges in the loop (but not in sub-loops) must point to loop header.
///   4. All sub-loops can be entered only from this loop and can exit only into this loop.
class Loop {
  friend std::vector<std::unique_ptr<Loop>> analyze_function_loops(
    Function* function,
    const DominatorTree& dominator_tree);

  Block* header_ = nullptr;
  std::unordered_set<Block*> blocks_;
  std::unordered_set<Block*> blocks_without_sub_loops_;

  std::unordered_set<Block*> back_edges_from_;
  std::vector<std::pair<Block*, Block*>> exiting_edges_;

  std::vector<std::unique_ptr<Loop>> sub_loops_;

  static bool find_loops_in_scc(Function* function,
                                const std::vector<Block*>& scc_vector,
                                const DominatorTree& dominator_tree,
                                SccContext<Block*>& scc_context,
                                std::vector<std::unique_ptr<Loop>>& loops);

  void debug_print_internal(const std::string& indentation) const;

 public:
  Block* header() const;

  const std::unordered_set<Block*>& blocks() const;
  const std::unordered_set<Block*>& blocks_without_sub_loops() const;

  const std::unordered_set<Block*>& back_edges_from() const;
  const std::vector<std::pair<Block*, Block*>>& exiting_edges() const;

  Block* single_back_edge() const;
  std::pair<Block*, Block*> single_exiting_edge() const;
  Block* single_exit_target() const;

  bool contains_block(Block* block) const;
  bool contains_block_skipping_sub_loops(Block* block) const;

  const std::vector<std::unique_ptr<Loop>>& sub_loops() const;

  void debug_print(const std::string& start_indentation = std::string()) const;
};

std::vector<std::unique_ptr<Loop>> analyze_function_loops(Function* function,
                                                          const DominatorTree& dominator_tree);
std::vector<std::unique_ptr<Loop>> analyze_function_loops(Function* function);

}  // namespace analysis

}  // namespace flugzeug