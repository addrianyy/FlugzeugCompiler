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

void Block::remove_phi_incoming() {
  size_t index = 0;

  // We have to watch out for iterator invalidation here.
  while (index < get_uses().size()) {
    User* user = get_uses()[index].user;
    if (Phi* phi = cast<Phi>(user)) {
      phi->remove_incoming(this);
    } else {
      index++;
    }
  }
}

void Block::destroy() {
  // If this block is used by branch instructions we cannot remove it. Value destructor will
  // throw an error.

  remove_phi_incoming();
  IntrusiveNode::destroy();
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