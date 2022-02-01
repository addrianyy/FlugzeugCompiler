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
    // Loop can be only entered via its header. If block other than header has non-loop
    // predecessor then it's an invalid loop.
    for (Block* predecessor : predecessors.find(block)->second) {
      if (!loop.blocks.contains(predecessor)) {
        return false;
      }
    }
  }

  auto& visited_state = dfs_context.visited[block];
  visited_state = DfsContext::State::Discovered;

  for (Block* successor : block->get_successors()) {
    // If this block jumps into non-loop block then it's an exiting block.
    if (!loop.blocks.contains(successor)) {
      loop.exiting_edges.emplace_back(block, successor);
      continue;
    }

    const auto successor_it = dfs_context.visited.find(successor);
    if (successor_it == dfs_context.visited.end()) {
      // Visit successor.
      if (!visit_loop_block(dfs_context, successor, loop, maybe_subloops_backedges, predecessors)) {
        return false;
      }
    } else {
      const auto& successor_state = successor_it->second;
      if (successor_state == DfsContext::State::Discovered) {
        // We have found a back edge in the graph. There are valid 2 possible scenarios:
        //   1. It's a back edge to the loop header.
        //   2. It's part of a sub-loop and it's a back edge to its loop header (we will verify this
        //      later in `find_loops_in_scc`).
        // In other cases this loop is invalid.

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

/// Returns `true` is loops were flattened (actual loop described by the SCC was invalid but
/// sub-loops were valid and were added to the loop list as non-child loops).
static bool
find_loops_in_scc(Function* function, const std::vector<Block*>& scc_vector,
                  const DominatorTree& dominator_tree,
                  const std::unordered_map<Block*, std::unordered_set<Block*>>& predecessors,
                  SccContext& scc_context, std::vector<std::unique_ptr<Loop>>& loops) {
  Loop loop;
  loop.blocks = std::unordered_set<Block*>(scc_vector.begin(), scc_vector.end());

  // Find the block that dominates all other blocks in the SCC. This will be potentially a loop
  // header.
  {
    // Start with randomly chosen block that is part of the SCC.
    loop.header = scc_vector.front();

    while (true) {
      // Get immediate dominator of potential header. If it's part of the loop then it will become
      // our new header.
      const auto dominator =
        const_cast<Block*>(dominator_tree.get_immediate_dominator(loop.header));
      if (loop.blocks.contains(dominator)) {
        loop.header = dominator;
      } else {
        break;
      }
    }
  }

  // Verify some loop properties using DFS traversal. Collect all exiting edges, back edges and
  // potential sub-loop back edges.
  DfsContext dfs_context{};
  std::vector<MaybeSubLoopBackedge> maybe_subloops_backedges;
  if (!visit_loop_block(dfs_context, loop.header, loop, maybe_subloops_backedges, predecessors)) {
    return false;
  }

  // Make sure that DFS visited all blocks in the loop. This should always happen as blocks are
  // strongly connected.
  for (Block* block : loop.blocks) {
    verify(dfs_context.visited.contains(block), "Not all loop blocks were visited using DFS");
  }

  // All back edges in this loop (but not in sub-loops) must branch to the loop header. By removing
  // header from the block set we make original loop non strongly connected. This will allow us to
  // find smaller SCCs that are inside loop.
  loop.blocks.erase(loop.header);
  const auto sub_sccs = calculate_sccs(scc_context, loop.blocks);
  loop.blocks.insert(loop.header);

  bool flattened = false;

  // Find sub-loops in the contained SCCs.
  std::vector<std::unique_ptr<Loop>> sub_loops;
  for (const auto& scc : sub_sccs) {
    flattened |=
      find_loops_in_scc(function, scc, dominator_tree, predecessors, scc_context, sub_loops);
  }

  const auto move_sub_loops_to_loops = [&loops, &sub_loops]() {
    loops.insert(loops.end(), std::make_move_iterator(sub_loops.begin()),
                 std::make_move_iterator(sub_loops.end()));
  };

  // It's possible that SCCs themselves described invalid loop which contained valid sub-loops. In
  // this case `sub_loops` will contain flattened, valid loops.
  //   this loop:
  //     (invalid loop)
  //       valid sub-loop of invalid loop
  // `sub_loops` here will contain valid sub-loop of invalid loop. But this case makes current loop
  // invalid as loop must have all their sub-loops valid. We have to return innermost loops.

  if (flattened) {
    move_sub_loops_to_loops();
    return true;
  }

  // Verify that backedges that aren't jumping to our header are actually part of subloops.
  if (!maybe_subloops_backedges.empty()) {
    // Go through every sub-loop recursively and check which back edges are part of sub-loops.
    for (const auto& sub_loop : sub_loops) {
      verify_subloops_backedges(*sub_loop, maybe_subloops_backedges);
    }

    // Make sure that all back edges that don't jump to our header are part of sub-loops.
    for (const auto& backedge : maybe_subloops_backedges) {
      if (!backedge.in_subloop) {
        move_sub_loops_to_loops();
        return true;
      }
    }
  }

  // Make sure that all sub-loops exit into our loop.
  for (const auto& sub_loop : sub_loops) {
    for (auto [exit_from, exit_to] : sub_loop->exiting_edges) {
      if (!loop.blocks.contains(exit_to)) {
        move_sub_loops_to_loops();
        return true;
      }
    }
  }

  loop.sub_loops = std::move(sub_loops);
  loops.push_back(std::make_unique<Loop>(std::move(loop)));

  return false;
}

std::vector<std::unique_ptr<Loop>> flugzeug::analyze_function_loops(Function* function) {
  // Loops are defined only for reachable blocks.
  const auto reachable_blocks =
    function->get_entry_block()->get_reachable_blocks_set(IncludeStart::Yes);

  DominatorTree dominator_tree(function);

  // Create map (block -> predecessors of block) for faster lookups.
  std::unordered_map<Block*, std::unordered_set<Block*>> predecessors;
  for (Block& block : *function) {
    predecessors.insert({&block, block.get_predecessors()});
  }

  std::vector<std::unique_ptr<Loop>> loops;
  SccContext scc_context{};

  // Calculate SCCs in the whole function. These contain potential loops (which may contain
  // sub-loops).
  const auto sccs = calculate_sccs(scc_context, reachable_blocks);

  // Find loops in the SCCs.
  for (const auto& scc : sccs) {
    find_loops_in_scc(function, scc, dominator_tree, predecessors, scc_context, loops);
  }

  return loops;
}