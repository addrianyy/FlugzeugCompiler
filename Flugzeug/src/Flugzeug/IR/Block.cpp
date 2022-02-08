#include "Block.hpp"
#include "ConsolePrinter.hpp"
#include "DebugPrinter.hpp"
#include "DominatorTree.hpp"
#include "Function.hpp"
#include "Instructions.hpp"

#include <deque>

using namespace flugzeug;

template <typename TBlock> TBlock* get_single_successor_generic(TBlock* block) {
  if (const auto branch = cast<Branch>(block->get_last_instruction())) {
    return branch->get_target();
  } else if (const auto cond_branch = cast<CondBranch>(block->get_last_instruction())) {
    const auto on_true = cond_branch->get_true_target();
    const auto on_false = cond_branch->get_false_target();

    if (on_true == on_false) {
      return on_true;
    }
  }

  return nullptr;
}

template <typename TBlock> TBlock* get_single_predecessor_generic(TBlock* block) {
  TBlock* predecessor = nullptr;

  for (auto& instruction : block->users<Instruction>()) {
    if (instruction.is_branching()) {
      const auto instruction_block = instruction.get_block();

      if (!predecessor) {
        predecessor = instruction_block;
      }

      if (predecessor != instruction_block) {
        return nullptr;
      }
    }
  }

  return predecessor;
}

template <typename TBlock> std::unordered_set<TBlock*> get_predecessors_generic(TBlock* block) {
  std::unordered_set<TBlock*> predecessors;
  predecessors.reserve(block->get_user_count());

  for (auto& instruction : block->users<Instruction>()) {
    if (instruction.is_branching()) {
      predecessors.insert(instruction.get_block());
    }
  }

  return predecessors;
}

template <typename TBlock, bool ReturnVector>
std::conditional_t<ReturnVector, std::vector<TBlock*>, std::unordered_set<TBlock*>>
traverse_generic(TBlock* start_block, TraversalType traversal) {
  const size_t reserve_count = std::min(size_t(8), start_block->get_function()->get_block_count());

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

      if (visited.contains(block)) {
        continue;
      }

      if (!first_iteration || with_start) {
        visited.insert(block);

        if constexpr (ReturnVector) {
          result.push_back(block);
        }
      }

      first_iteration = false;

      for (TBlock* successor : block->successors()) {
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

      if (visited.contains(block)) {
        continue;
      }

      if (!first_iteration || with_start) {
        visited.insert(block);

        if constexpr (ReturnVector) {
          result.push_back(block);
        }
      }

      first_iteration = false;

      for (TBlock* successor : block->successors()) {
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

  invalid_instruction_order = true;
}

void Block::on_removed_node(Instruction* instruction) { (void)instruction; }

void Block::update_instruction_order() const {
  if (invalid_instruction_order) {
    size_t index = 0;
    for (const Instruction& instruction : *this) {
      instruction.order_in_block = index++;
    }

    invalid_instruction_order = false;
  }
}

Block::~Block() {
  verify(instruction_list.is_empty(), "Cannot remove non-empty block.");
  verify(!get_function(), "Cannot remove block that is attached to the function.");
}

void Block::print(IRPrinter& printer) const {
  {
    auto p = printer.create_line_printer();
    p.print(this, IRPrinter::Item::Colon);
  }

  for (const Instruction& instruction : instruction_list) {
    printer.tab();
    instruction.print(printer);
  }
}

void Block::print() const {
  ConsolePrinter printer(ConsolePrinter::Variant::ColorfulIfSupported);
  print(printer);
}

void Block::debug_print() const {
  DebugPrinter printer;
  print(printer);
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
  for (Phi& phi : dont_invalidate_current(users<Phi>())) {
    phi.remove_incoming(this);
  }

  IntrusiveNode::destroy();
}

void Block::replace_incoming_blocks_in_phis(const Block* old_incoming, Block* new_incoming) {
  if (is_entry_block()) {
    return;
  }

  for (Phi& phi : instructions<Phi>()) {
    phi.replace_incoming_block_opt(old_incoming, new_incoming);
  }
}

void Block::remove_incoming_block_from_phis(const Block* incoming, bool destroy_empty_phis) {
  if (is_entry_block()) {
    return;
  }

  for (Phi& phi : dont_invalidate_current(instructions<Phi>())) {
    if (phi.remove_incoming_opt(incoming) && destroy_empty_phis && phi.is_empty()) {
      phi.destroy();
    }
  }
}

void Block::on_removed_branch_to(Block* to, bool destroy_empty_phis) const {
  // We don't want to do anything if we haven't actually removed CFG edge between `this` and `to`.
  if (!has_successor(to)) {
    to->remove_incoming_block_from_phis(this, destroy_empty_phis);
  }
}

bool Block::is_terminated() const {
  const auto last = get_last_instruction();
  return last && last->is_terminator();
}

bool Block::dominates(const Block* other, const DominatorTree& dominator_tree) const {
  return dominator_tree.first_dominates_second(this, other);
}

bool Block::is_dominated_by(const Block* other, const DominatorTree& dominator_tree) const {
  return other->dominates(this, dominator_tree);
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
  for (const Instruction& instruction : users<Instruction>()) {
    if (instruction.is_branching() && instruction.get_block() == predecessor) {
      return true;
    }
  }

  return false;
}

Block* Block::get_single_successor() { return get_single_successor_generic<Block>(this); }

const Block* Block::get_single_successor() const {
  return get_single_successor_generic<const Block>(this);
}

Block* Block::get_single_predecessor() { return get_single_predecessor_generic<Block>(this); }

const Block* Block::get_single_predecessor() const {
  return get_single_predecessor_generic<const Block>(this);
}

BlockTargets<Block> Block::successors() {
  if (const auto terminator = get_last_instruction()) {
    return terminator->targets();
  }
  return {};
}

BlockTargets<const Block> Block::successors() const {
  if (const auto terminator = get_last_instruction()) {
    return terminator->targets();
  }
  return {};
}

std::unordered_set<Block*> Block::predecessors() { return get_predecessors_generic<Block>(this); }

std::unordered_set<const Block*> Block::predecessors() const {
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