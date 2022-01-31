#include "Loops.hpp"
#include "SCC.hpp"

#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>

using namespace flugzeug;

struct DfsContext {
  enum State {
    Discovered,
    Finished,
  };
  std::unordered_map<Block*, State> visited;
};

struct MaybeSubLoopBackedge {
  Block* from = nullptr;
  Block* to = nullptr;
  bool in_subloop = false;
};

static bool
visit_loop_block(DfsContext& dfs_context, Block* block, Loop& loop,
                 std::vector<MaybeSubLoopBackedge>& maybe_subloops_backedges,
                 const std::unordered_map<Block*, std::unordered_set<Block*>>& predecessors) {
  verify(!dfs_context.visited.contains(block),
         "Running `visit_loop_block` on already visited block");

  if (block != loop.header) {
    // Loop can be only entered via `loop.header`. If block other than header has non-loop
    // predecessor it's an invalid loop.
    for (Block* predecessor : predecessors.find(block)->second) {
      if (!loop.blocks.contains(predecessor)) {
        return false;
      }
    }
  }

  auto& visited_state = dfs_context.visited[block];
  visited_state = DfsContext::State::Discovered;

  for (Block* successor : block->get_successors()) {
    if (!loop.blocks.contains(successor)) {
      loop.exiting_edges.emplace_back(block, successor);
      continue;
    }

    const auto successor_it = dfs_context.visited.find(successor);
    if (successor_it == dfs_context.visited.end()) {
      if (!visit_loop_block(dfs_context, successor, loop, maybe_subloops_backedges, predecessors)) {
        return false;
      }
    } else {
      const auto& successor_state = successor_it->second;
      if (successor_state == DfsContext::State::Discovered) {
        // This is back edge. It must be branch to the header (unless it's back edge of sub-loop).
        if (successor != loop.header) {
          maybe_subloops_backedges.push_back(MaybeSubLoopBackedge{block, successor});
        } else {
          loop.back_edges_from.insert(block);
        }
      }
    }
  }

  visited_state = DfsContext::State::Finished;
  return true;
}

static void verify_subloops_backedges(const Loop& subloop,
                                      std::vector<MaybeSubLoopBackedge>& maybe_subloops_backedges) {
  for (auto& maybe_backedge : maybe_subloops_backedges) {
    if (maybe_backedge.to == subloop.header && subloop.blocks.contains(maybe_backedge.from)) {
      maybe_backedge.in_subloop = true;
    }
  }

  for (const auto& subsubloop : subloop.sub_loops) {
    verify_subloops_backedges(*subsubloop, maybe_subloops_backedges);
  }
}

static void
get_loops_in_scc(Function* function, const std::vector<Block*>& scc_vector,
                 const DominatorTree& dominator_tree,
                 const std::unordered_map<Block*, std::unordered_set<Block*>>& predecessors,
                 SccContext& scc_context, std::vector<std::unique_ptr<Loop>>& loops) {
  Loop loop;
  loop.header = scc_vector.front();
  loop.blocks = std::unordered_set<Block*>(scc_vector.begin(), scc_vector.end());

  // Find the block that dominates all other block in the SCC. This will be potentially loop header.
  while (true) {
    const auto dominator = const_cast<Block*>(dominator_tree.get_immediate_dominator(loop.header));
    if (loop.blocks.contains(dominator)) {
      loop.header = dominator;
    } else {
      break;
    }
  }

  DfsContext dfs_context{};
  std::vector<MaybeSubLoopBackedge> maybe_subloops_backedges;
  if (!visit_loop_block(dfs_context, loop.header, loop, maybe_subloops_backedges, predecessors)) {
    return;
  }

  for (Block* block : loop.blocks) {
    verify(dfs_context.visited.contains(block), "Not all loop blocks were visited using DFS");
  }

  loop.blocks.erase(loop.header);
  const auto sub_sccs = calculate_sccs(scc_context, loop.blocks);
  loop.blocks.insert(loop.header);

  std::vector<std::unique_ptr<Loop>> sub_loops;
  for (const auto& scc : sub_sccs) {
    get_loops_in_scc(function, scc, dominator_tree, predecessors, scc_context, sub_loops);
  }

  // Verify that backedges that aren't jumping to our header are actually part of subloops.
  if (!maybe_subloops_backedges.empty()) {
    for (const auto& sub_loop : sub_loops) {
      verify_subloops_backedges(*sub_loop, maybe_subloops_backedges);
    }

    for (const auto& backedge : maybe_subloops_backedges) {
      // This loop is invalid, return sub-loops.
      if (!backedge.in_subloop) {
        loops.insert(loops.end(), std::make_move_iterator(sub_loops.begin()),
                     std::make_move_iterator(sub_loops.end()));
        return;
      }
    }
  }

  const auto remove_improperly_nested_predicate = [&loop](const std::unique_ptr<Loop>& sub_loop) {
    for (auto [exit_from, exit_to] : sub_loop->exiting_edges) {
      if (!loop.blocks.contains(exit_to)) {
        return true;
      }
    }

    return false;
  };

  // Remove sub-loops which't don't exit to our loop.
  sub_loops.erase(
    std::remove_if(sub_loops.begin(), sub_loops.end(), remove_improperly_nested_predicate),
    sub_loops.end());

  loop.sub_loops = std::move(sub_loops);
  loops.push_back(std::make_unique<Loop>(std::move(loop)));
}

std::vector<std::unique_ptr<Loop>> flugzeug::analyze_function_loops(Function* function) {
  const auto reachable_blocks =
    function->get_entry_block()->get_reachable_blocks_set(IncludeStart::Yes);

  DominatorTree dominator_tree(function);

  std::unordered_map<Block*, std::unordered_set<Block*>> predecessors;
  for (Block& block : *function) {
    predecessors.insert({&block, block.get_predecessors()});
  }

  std::vector<std::unique_ptr<Loop>> loops;
  SccContext scc_context{};

  const auto sccs = calculate_sccs(scc_context, reachable_blocks);
  for (const auto& scc : sccs) {
    get_loops_in_scc(function, scc, dominator_tree, predecessors, scc_context, loops);
  }

  return loops;
}