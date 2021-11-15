#include "Block.hpp"
#include "Function.hpp"
#include "Instructions.hpp"

#include <deque>

using namespace flugzeug;

template <typename TBlock> TBlock* get_single_predecessor_generic(TBlock* block) {
  TBlock* predecessor = nullptr;

  for (auto& user : block->get_users()) {
    if (cast<Branch>(user) || cast<CondBranch>(user)) {
      const auto user_block = cast<Instruction>(user)->get_block();

      if (!predecessor) {
        predecessor = user_block;
      }

      if (predecessor != user_block) {
        return nullptr;
      }
    }
  }

  return predecessor;
}

template <typename TBlock> std::unordered_set<TBlock*> get_predecessors_generic(TBlock* block) {
  std::unordered_set<TBlock*> predecessors;
  predecessors.reserve(block->get_user_count());

  for (auto& user : block->get_users()) {
    if (cast<Branch>(user) || cast<CondBranch>(user)) {
      predecessors.insert(cast<Instruction>(user)->get_block());
    }
  }

  return predecessors;
}

template <typename TBlock, bool ReturnVector>
std::conditional_t<ReturnVector, std::vector<TBlock*>, std::unordered_set<TBlock*>>
traverse_generic(TBlock* start_block, TraversalType traversal) {
  const size_t reserve_count = std::min(8ull, start_block->get_function()->get_block_count());

  std::vector<TBlock*> result;
  std::unordered_set<TBlock*> visited;

  if constexpr (ReturnVector) {
    result.reserve(reserve_count);
  }

  visited.reserve(reserve_count);

  const bool with_start =
    traversal == TraversalType::BFS_WithStart || traversal == TraversalType::DFS_WithStart;
  bool first_iteration = true;

  if (traversal == TraversalType::BFS_WithStart || traversal == TraversalType::BFS_WithoutStart) {
    // BFS

    std::deque<TBlock*> queue;
    queue.push_back(start_block);

    while (!queue.empty()) {
      TBlock* block = queue.front();
      queue.pop_front();

      if (!first_iteration || with_start) {
        visited.insert(block);

        if constexpr (ReturnVector) {
          result.push_back(block);
        }
      }

      first_iteration = false;

      for (TBlock* successor : block->get_successors()) {
        if (!visited.contains(successor)) {
          queue.push_back(successor);
        }
      }
    }
  } else if (traversal == TraversalType::DFS_WithStart ||
             traversal == TraversalType::DFS_WithoutStart) {
    // DFS

    std::vector<TBlock*> stack;
    stack.reserve(reserve_count);
    stack.push_back(start_block);

    while (!stack.empty()) {
      TBlock* block = stack.back();
      stack.pop_back();

      if (!first_iteration || with_start) {
        visited.insert(block);

        if constexpr (ReturnVector) {
          result.push_back(block);
        }
      }

      first_iteration = false;

      for (TBlock* successor : block->get_successors()) {
        if (!visited.contains(successor)) {
          stack.push_back(successor);
        }
      }
    }
  } else {
    unreachable();
  }

  if constexpr (ReturnVector) {
    return result;
  } else {
    return visited;
  }
}

void Block::on_added_node(Instruction* instruction) {
  if (get_function() && !instruction->is_void()) {
    instruction->set_display_index(get_function()->allocate_value_index());
  }
}

void Block::on_removed_node(Instruction* instruction) { (void)instruction; }

void Block::remove_all_references_in_phis() {
  for (User& user : dont_invalidate_current(get_users())) {
    if (const auto phi = cast<Phi>(user)) {
      phi->remove_incoming(this);
    }
  }
}

Block::~Block() {
  verify(instructions.is_empty(), "Cannot remove non-empty block.");
  verify(!get_function(), "Cannot remove block that is attached to the function.");
}

void Block::print(IRPrinter& printer) const {
  {
    auto p = printer.create_line_printer();
    p.print(this, IRPrinter::Item::Colon);
  }

  for (const Instruction& instruction : instructions) {
    printer.tab();
    instruction.print(printer);
  }
}

std::string Block::format() const {
  return is_entry ? "entry" : ("block_" + std::to_string(get_display_index()));
}

void Block::clear() {
  while (!is_empty()) {
    get_first_instruction()->destroy();
  }
}

void Block::destroy() {
  // If this block is used by branch instructions we cannot remove it. Value destructor will
  // throw an error.

  // Remove all incoming values in Phis that use this block.
  remove_all_references_in_phis();
  IntrusiveNode::destroy();
}

void Block::replace_incoming_blocks_in_phis(const Block* old_incoming, Block* new_incoming) {
  if (is_entry_block()) {
    return;
  }

  for (Instruction& instruction : *this) {
    if (const auto phi = cast<Phi>(instruction)) {
      phi->replace_incoming_block_opt(old_incoming, new_incoming);
    }
  }
}

void Block::remove_incoming_block_from_phis(Block* incoming, bool destroy_empty_phis) {
  if (is_entry_block()) {
    return;
  }

  for (Instruction& instruction : dont_invalidate_current(*this)) {
    if (const auto phi = cast<Phi>(instruction)) {
      if (phi->remove_incoming_opt(incoming) && destroy_empty_phis && phi->is_empty()) {
        phi->destroy();
      }
    }
  }
}

void Block::on_removed_branch_to(Block* to, bool destroy_empty_phis) {
  // We don't want to do anything if we haven't actually removed CFG edge between `this` and `to`.
  if (!has_successor(to)) {
    to->remove_incoming_block_from_phis(this, destroy_empty_phis);
  }
}

bool Block::has_successor(const Block* successor) const {
  const auto terminator = get_last_instruction();
  if (const auto branch = cast<Branch>(terminator)) {
    return branch->get_target() == successor;
  } else if (const auto cond_branch = cast<CondBranch>(terminator)) {
    return cond_branch->get_true_target() == successor ||
           cond_branch->get_false_target() == successor;
  }

  return false;
}

bool Block::has_predecessor(const Block* predecessor) const {
  for (const User& user : get_users()) {
    if (cast<Branch>(user) || cast<CondBranch>(user)) {
      if (cast<Instruction>(user)->get_block() == predecessor) {
        return true;
      }
    }
  }

  return false;
}

Block* Block::get_single_predecessor() { return get_single_predecessor_generic<Block>(this); }

const Block* Block::get_single_predecessor() const {
  return get_single_predecessor_generic<const Block>(this);
}

BlockTargets<Block> Block::get_successors() {
  if (const auto terminator = get_last_instruction()) {
    return terminator->get_targets();
  }
  return {};
}

BlockTargets<const Block> Block::get_successors() const {
  if (const auto terminator = get_last_instruction()) {
    return terminator->get_targets();
  }
  return {};
}

std::unordered_set<Block*> Block::get_predecessors() {
  return get_predecessors_generic<Block>(this);
}

std::unordered_set<const Block*> Block::get_predecessors() const {
  return get_predecessors_generic<const Block>(this);
}

std::vector<Block*> Block::get_reachable_blocks(TraversalType traversal) {
  return traverse_generic<Block, true>(this, traversal);
}

std::vector<const Block*> Block::get_reachable_blocks(TraversalType traversal) const {
  return traverse_generic<const Block, true>(this, traversal);
}

std::unordered_set<Block*> Block::get_reachable_blocks_set(IncludeStart include_start) {
  // Prefer DFS as it's faster.
  return traverse_generic<Block, false>(this, include_start == IncludeStart::Yes
                                                ? TraversalType::DFS_WithStart
                                                : TraversalType::DFS_WithoutStart);
}

std::unordered_set<const Block*> Block::get_reachable_blocks_set(IncludeStart include_start) const {
  // Prefer DFS as it's faster.
  return traverse_generic<const Block, false>(this, include_start == IncludeStart::Yes
                                                      ? TraversalType::DFS_WithStart
                                                      : TraversalType::DFS_WithoutStart);
}