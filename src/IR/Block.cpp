#include "Block.hpp"
#include "Function.hpp"
#include "Instructions.hpp"

#include <deque>

using namespace flugzeug;

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

template <typename TBlock>
std::vector<TBlock*> traverse_generic(TBlock* start_block, TraversalType traversal) {
  const size_t result_reserve = 8;
  const size_t visited_reserve = 8;
  const size_t worklist_reserve = 8;

  std::vector<TBlock*> result;
  std::unordered_set<TBlock*> visited;

  result.reserve(result_reserve);
  visited.reserve(visited_reserve);

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
        result.push_back(block);
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
    stack.reserve(worklist_reserve);
    stack.push_back(start_block);

    while (!stack.empty()) {
      TBlock* block = stack.back();
      stack.pop_back();

      if (!first_iteration || with_start) {
        visited.insert(block);
        result.push_back(block);
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

  return result;
}

void Block::on_added_node(Instruction* instruction) {
  if (get_function() && !instruction->is_void()) {
    instruction->set_display_index(get_function()->allocate_value_index());
  }
}

void Block::on_removed_node(Instruction* instruction) { (void)instruction; }

void Block::remove_all_references_in_phis() {
  const auto& uses = get_uses();

  size_t index = 0;

  // We have to watch out for iterator invalidation here.
  while (index < uses.size()) {
    if (const auto phi = cast<Phi>(uses[index].user)) {
      phi->remove_incoming(this);
    } else {
      index++;
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

void Block::remove_incoming_block_from_phis(Block* incoming) {
  if (is_entry_block()) {
    return;
  }

  for (Instruction& instruction : *this) {
    if (const auto phi = cast<Phi>(instruction)) {
      phi->remove_incoming_opt(incoming);
      // TODO: Remove Phi if empty?
    }
  }
}

void Block::on_removed_branch_to(Block* to) {
  // We don't want to do anything if we haven't actually removed CFG edge between `this` and `to`.
  if (!has_successor(to)) {
    to->remove_incoming_block_from_phis(this);
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
  for (auto& user : get_users()) {
    if (cast<Branch>(user) || cast<CondBranch>(user)) {
      if (cast<Instruction>(user)->get_block() == predecessor) {
        return true;
      }
    }
  }

  return false;
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
  return traverse_generic<Block>(this, traversal);
}

std::vector<const Block*> Block::get_reachable_blocks(TraversalType traversal) const {
  return traverse_generic<const Block>(this, traversal);
}
