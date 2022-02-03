#include "Loops.hpp"
#include "SCC.hpp"

#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>

#include <Flugzeug/Core/Log.hpp>

using namespace flugzeug;

struct DfsContext {
  enum State {
    Discovered,
    Finished,
  };
  std::unordered_map<Block*, State> block_state;
};

struct MaybeSubLoopBackedge {
  Block* from = nullptr;
  Block* to = nullptr;
  bool in_subloop = false;
};

static bool
visit_loop_block(DfsContext& dfs_context, Block* block, Loop& loop,
                 std::vector<std::pair<Block*, Block*>>& exiting_edges,
                 std::unordered_set<Block*>& back_edges_from,
                 std::vector<MaybeSubLoopBackedge>& maybe_subloops_backedges,
                 const std::unordered_map<Block*, std::unordered_set<Block*>>& predecessors) {
  verify(!dfs_context.block_state.contains(block),
         "Running `visit_loop_block` on already visited block");

  if (block != loop.get_header()) {
    // Loop can be only entered via its header. If block other than header has non-loop
    // predecessor then it's an invalid loop.
    for (Block* predecessor : predecessors.find(block)->second) {
      if (!loop.contains_block(predecessor)) {
        return false;
      }
    }
  }

  auto& block_state = dfs_context.block_state[block];
  block_state = DfsContext::State::Discovered;

  for (Block* successor : block->successors()) {
    // If this block jumps into non-loop block then it's an exiting block.
    if (!loop.contains_block(successor)) {
      exiting_edges.emplace_back(block, successor);
      continue;
    }

    const auto successor_it = dfs_context.block_state.find(successor);
    if (successor_it == dfs_context.block_state.end()) {
      // Visit successor.
      if (!visit_loop_block(dfs_context, successor, loop, exiting_edges, back_edges_from,
                            maybe_subloops_backedges, predecessors)) {
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

        if (successor != loop.get_header()) {
          maybe_subloops_backedges.push_back(MaybeSubLoopBackedge{block, successor});
        } else {
          back_edges_from.insert(block);
        }
      }
    }
  }

  block_state = DfsContext::State::Finished;
  return true;
}

static void verify_subloops_backedges(const Loop& subloop,
                                      std::vector<MaybeSubLoopBackedge>& maybe_subloops_backedges) {
  for (auto& maybe_backedge : maybe_subloops_backedges) {
    if (maybe_backedge.to == subloop.get_header() && subloop.contains_block(maybe_backedge.from)) {
      maybe_backedge.in_subloop = true;
    }
  }

  for (const auto& subsubloop : subloop.get_sub_loops()) {
    verify_subloops_backedges(*subsubloop, maybe_subloops_backedges);
  }
}

/// Returns `true` is loops were flattened (actual loop described by the SCC was invalid but
/// sub-loops were valid and were added to the loop list as non-child loops).
bool Loop::find_loops_in_scc(
  Function* function, const std::vector<Block*>& scc_vector, const DominatorTree& dominator_tree,
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
  if (!visit_loop_block(dfs_context, loop.header, loop, loop.exiting_edges, loop.back_edges_from,
                        maybe_subloops_backedges, predecessors)) {
    return false;
  }

  // Make sure that DFS visited all blocks in the loop. This should always happen as blocks are
  // strongly connected.
  for (Block* block : loop.blocks) {
    verify(dfs_context.block_state.contains(block), "Not all loop blocks were visited using DFS");
  }

  // All back edges in this loop (but not in sub-loops) must branch to the loop header. By removing
  // header from the block set we make original loop non strongly connected. This will allow us to
  // find smaller SCCs that are inside the loop.
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
  // this case `sub_loops` will contain innermost loops.
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

  loop.blocks_without_sub_loops = loop.blocks;
  for (const auto& sub_loop : loop.sub_loops) {
    for (Block* block : sub_loop->blocks) {
      loop.blocks_without_sub_loops.erase(block);
    }
  }

  loops.push_back(std::make_unique<Loop>(std::move(loop)));

  return false;
}

std::vector<std::unique_ptr<Loop>>
flugzeug::analyze_function_loops(Function* function, const DominatorTree& dominator_tree) {
  // Loops are defined only for reachable blocks.
  const auto reachable_blocks =
    function->get_entry_block()->get_reachable_blocks_set(IncludeStart::Yes);

  // Create map (block -> predecessors of block) for faster lookups.
  std::unordered_map<Block*, std::unordered_set<Block*>> predecessors;
  for (Block& block : *function) {
    predecessors.insert({&block, block.predecessors()});
  }

  std::vector<std::unique_ptr<Loop>> loops;
  SccContext scc_context{};

  // Calculate SCCs in the whole function. These contain potential loops (which may contain
  // sub-loops).
  const auto sccs = calculate_sccs(scc_context, reachable_blocks);

  // Find loops in the SCCs.
  for (const auto& scc : sccs) {
    Loop::find_loops_in_scc(function, scc, dominator_tree, predecessors, scc_context, loops);
  }

  return loops;
}

std::vector<std::unique_ptr<Loop>> flugzeug::analyze_function_loops(Function* function) {
  DominatorTree dominator_tree(function);
  return analyze_function_loops(function, dominator_tree);
}

Block* Loop::get_header() const { return header; }

const std::unordered_set<Block*>& Loop::get_blocks() const { return blocks; }
const std::unordered_set<Block*>& Loop::get_blocks_without_sub_loops() const {
  return blocks_without_sub_loops;
}

const std::unordered_set<Block*>& Loop::get_back_edges_from() const { return back_edges_from; }
const std::vector<std::pair<Block*, Block*>>& Loop::get_exiting_edges() const {
  return exiting_edges;
}

Block* Loop::get_single_back_edge() const {
  if (back_edges_from.size() == 1) {
    return *back_edges_from.begin();
  }

  return nullptr;
}

std::pair<Block*, Block*> Loop::get_single_exiting_edge() const {
  if (exiting_edges.size() == 1) {
    return exiting_edges[0];
  }

  return {};
}

bool Loop::contains_block(Block* block) const { return blocks.contains(block); }
bool Loop::contains_block_skipping_sub_loops(Block* block) const {
  return blocks_without_sub_loops.contains(block);
}

const std::vector<std::unique_ptr<Loop>>& Loop::get_sub_loops() const { return sub_loops; }

void Loop::debug_print_internal(const std::string& indentation) const {
  std::string blocks_s;
  std::string blocks_without_sub_loops_s;
  std::string back_edges_from_s;
  std::string exiting_edges_s;

  for (Block* block : get_blocks()) {
    blocks_s += block->format() + ", ";
  }

  for (Block* block : get_blocks_without_sub_loops()) {
    blocks_without_sub_loops_s += block->format() + ", ";
  }

  for (Block* block : get_back_edges_from()) {
    back_edges_from_s += block->format() + ", ";
  }

  for (const auto [from, to] : get_exiting_edges()) {
    exiting_edges_s += fmt::format("({} -> {}), ", from->format(), to->format());
  }

  const auto remove_trailing_comma = [](std::string& s) {
    if (s.ends_with(", ")) {
      s = s.substr(0, s.size() - 2);
    }
  };

  remove_trailing_comma(blocks_s);
  remove_trailing_comma(blocks_without_sub_loops_s);
  remove_trailing_comma(back_edges_from_s);
  remove_trailing_comma(exiting_edges_s);

  log_debug("{}Loop {}", indentation, get_header()->format());
  log_debug("{}  blocks: {}", indentation, blocks_s);
  if (!get_sub_loops().empty()) {
    log_debug("{}  blocks (no sub-loops): {}", indentation, blocks_without_sub_loops_s);
  }
  log_debug("{}  back edges from: {}", indentation, back_edges_from_s);
  log_debug("{}  exiting edges: {}", indentation, exiting_edges_s);

  if (!get_sub_loops().empty()) {
    log_debug("{}  sub loops:", indentation);

    for (const auto& sub_loop : get_sub_loops()) {
      sub_loop->debug_print_internal(indentation + "    ");
    }
  }
}

void Loop::debug_print(const std::string& start_indentation) const {
  debug_print_internal(start_indentation);
}
