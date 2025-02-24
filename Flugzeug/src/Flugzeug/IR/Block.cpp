#include "Block.hpp"
#include "ConsoleIRPrinter.hpp"
#include "DominatorTree.hpp"
#include "Function.hpp"
#include "Instructions.hpp"
#include "Internal/DebugIRPrinter.hpp"

#include <algorithm>
#include <deque>

using namespace flugzeug;

template <typename TBlock>
TBlock* get_single_successor_generic(TBlock* block) {
  if (const auto branch = cast<Branch>(block->last_instruction())) {
    return branch->target();
  } else if (const auto cond_branch = cast<CondBranch>(block->last_instruction())) {
    const auto on_true = cond_branch->true_target();
    const auto on_false = cond_branch->false_target();

    if (on_true == on_false) {
      return on_true;
    }
  }

  return nullptr;
}

template <typename TBlock, bool ReturnVector>
std::conditional_t<ReturnVector, std::vector<TBlock*>, std::unordered_set<TBlock*>>
traverse_generic(TBlock* start_block, TraversalType traversal) {
  const size_t block_count = start_block->function()->block_count();

  size_t reserve_count = start_block->function()->block_count() / 8;
  if (reserve_count < 4) {
    reserve_count = start_block->function()->block_count();
  }

  std::vector<TBlock*> result;
  std::unordered_set<TBlock*> visited;

  if constexpr (ReturnVector) {
    result.reserve(block_count);
  }

  visited.reserve(block_count);

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

static Block* get_branch_block(User* user) {
  if (const auto instruction = cast<Instruction>(user)) {
    if (instruction->is_branching()) {
      return instruction->block();
    }
  }

  return nullptr;
}

static void remove_block_from_vector(std::vector<Block*>& blocks, Block* block) {
  const auto it = std::find(blocks.begin(), blocks.end(), block);
  verify(it != blocks.end(), "Failed to find block in the block list");

  const auto index = std::distance(blocks.begin(), it);

  if (blocks.size() > 1) {
    std::iter_swap(blocks.begin() + index, blocks.end() - 1);
    blocks.pop_back();
  } else {
    blocks.clear();
  }
}

void Block::on_added_node(Instruction* instruction) {
  if (function() && !instruction->is_void()) {
    instruction->set_display_index(function()->allocate_value_index());
  }

  if (instruction->is_branching()) {
    for (Value& operand : instruction->operands()) {
      const auto target_block = cast<Block>(operand);
      if (target_block) {
        target_block->add_predecessor(this);
      }
    }
  }

  invalid_instruction_order = true;
}

void Block::on_removed_node(Instruction* instruction) {
  if (instruction->is_branching()) {
    for (Value& operand : instruction->operands()) {
      const auto target_block = cast<Block>(operand);
      if (target_block) {
        target_block->remove_predecessor(this);
      }
    }
  }
}

void Block::on_added_block_user(User* user) {
  if (const auto block = get_branch_block(user)) {
    add_predecessor(block);
  }
}

void Block::on_removed_block_user(User* user) {
  if (const auto block = get_branch_block(user)) {
    remove_predecessor(block);
  }
}

void Block::add_predecessor(Block* predecessor) {
  if (std::find(predecessors_list_unique.begin(), predecessors_list_unique.end(), predecessor) ==
      predecessors_list_unique.end()) {
    predecessors_list_unique.push_back(predecessor);
  }
  predecessors_list.push_back(predecessor);
}

void Block::remove_predecessor(Block* predecessor) {
  remove_block_from_vector(predecessors_list, predecessor);

  if (std::find(predecessors_list.begin(), predecessors_list.end(), predecessor) ==
      predecessors_list.end()) {
    remove_block_from_vector(predecessors_list_unique, predecessor);
  }
}

void Block::update_instruction_order() const {
  if (invalid_instruction_order) {
    size_t index = 0;
    for (const Instruction& instruction : *this) {
      instruction.order_in_block_ = index++;
    }

    invalid_instruction_order = false;
  }
}

std::unordered_set<const Value*> Block::get_inlinable_values() const {
  std::unordered_set<const Value*> inlinable;
  std::unordered_map<const Value*, uint32_t> values_complexity;

  for (const Instruction& instruction : *this) {
    if (instruction.is_void() || instruction.is_volatile() || cast<Load>(instruction) ||
        cast<Phi>(instruction) || cast<StackAlloc>(instruction)) {
      continue;
    }

    if (instruction.user_count() == 0 || instruction.user_count() > 3) {
      continue;
    }

    bool non_inlineable = false;

    for (const Instruction& user : instruction.users<Instruction>()) {
      if (user.block() != this) {
        non_inlineable = true;
        break;
      }
    }

    if (non_inlineable) {
      continue;
    }

    uint32_t complexity = 1;
    for (const Value& operand : instruction.operands()) {
      const auto operand_complexity = values_complexity.find(&operand);
      if (operand_complexity != values_complexity.end()) {
        complexity += operand_complexity->second;
      }
    }

    if (complexity > 6) {
      continue;
    }

    inlinable.insert(&instruction);
    values_complexity.insert({&instruction, complexity});
  }

  return inlinable;
}

Block::~Block() {
  verify(instruction_list.empty(), "Cannot remove non-empty block.");
  verify(!function(), "Cannot remove block that is attached to the function.");

  verify(predecessors_list.empty(), "Predecessors list is not empty.");
  verify(predecessors_list_unique.empty(), "Unique predecessors list is not empty.");
}

void Block::print(IRPrinter& printer, IRPrintingMethod method) const {
  {
    auto p = printer.create_line_printer();
    p.print(this, IRPrinter::Item::Colon);
  }

  if (method == IRPrintingMethod::Standard) {
    for (const Instruction& instruction : instruction_list) {
      printer.tab();
      instruction.print(printer);
    }
  } else {
    const auto inlined = get_inlinable_values();

    for (const Instruction& instruction : instruction_list) {
      if (inlined.contains(&instruction)) {
        continue;
      }

      printer.tab();
      instruction.print_compact(printer, inlined);
    }
  }
}

void Block::print(IRPrintingMethod method) const {
  ConsoleIRPrinter printer(ConsoleIRPrinter::Variant::ColorfulIfSupported);
  print(printer, method);
}

void Block::debug_print() const {
  DebugIRPrinter printer;
  print(printer);
}

std::string Block::format() const {
  return is_entry ? "entry" : ("block_" + std::to_string(display_index()));
}

void Block::clear() {
  while (!empty()) {
    first_instruction()->destroy();
  }
}

void Block::destroy() {
  // If this block is used by branch instructions we cannot remove it. Value destructor will
  // throw an error.

  // Remove all incoming values in Phis that use this block.
  for (Phi& phi : advance_early(users<Phi>())) {
    phi.remove_incoming(this);
  }

  IntrusiveNode::destroy();
}

void Block::clear_and_destroy() {
  clear();
  destroy();
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

  for (Phi& phi : advance_early(instructions<Phi>())) {
    if (phi.remove_incoming_opt(incoming) && destroy_empty_phis && phi.empty()) {
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
  const auto last = last_instruction();
  return last && last->is_terminator();
}

bool Block::dominates(const Block* other, const DominatorTree& dominator_tree) const {
  return dominator_tree.first_dominates_second(this, other);
}

bool Block::is_dominated_by(const Block* other, const DominatorTree& dominator_tree) const {
  return other->dominates(this, dominator_tree);
}

bool Block::has_successor(const Block* successor) const {
  const auto terminator = last_instruction();
  if (const auto branch = cast<Branch>(terminator)) {
    return branch->target() == successor;
  } else if (const auto cond_branch = cast<CondBranch>(terminator)) {
    return cond_branch->true_target() == successor || cond_branch->false_target() == successor;
  }

  return false;
}

bool Block::has_predecessor(const Block* predecessor) const {
  return std::find(predecessors_list_unique.begin(), predecessors_list_unique.end(), predecessor) !=
         predecessors_list_unique.end();
}

Block* Block::single_successor() {
  return get_single_successor_generic<Block>(this);
}

const Block* Block::single_successor() const {
  return get_single_successor_generic<const Block>(this);
}

Block* Block::single_predecessor() {
  return predecessors_list_unique.size() == 1 ? predecessors_list_unique[0] : nullptr;
}

const Block* Block::single_predecessor() const {
  return predecessors_list_unique.size() == 1 ? predecessors_list_unique[0] : nullptr;
}

BlockTargets<Block> Block::successors() {
  if (const auto terminator = last_instruction()) {
    return terminator->targets();
  }
  return {};
}

BlockTargets<const Block> Block::successors() const {
  if (const auto terminator = last_instruction()) {
    return terminator->targets();
  }
  return {};
}

std::span<Block*> Block::predecessors() {
  return predecessors_list_unique;
}

std::span<const Block*> Block::predecessors() const {
  const auto const_predecessors = const_cast<const Block**>(predecessors_list_unique.data());
  return std::span<const Block*>{const_predecessors, predecessors_list_unique.size()};
}

std::unordered_set<Block*> Block::predecessors_set() {
  std::unordered_set<Block*> set;
  set.reserve(predecessors().size());

  for (Block* block : predecessors()) {
    set.insert(block);
  }

  return set;
}

std::unordered_set<const Block*> Block::predecessors_set() const {
  std::unordered_set<const Block*> set;
  set.reserve(predecessors().size());

  for (const Block* block : predecessors()) {
    set.insert(block);
  }

  return set;
}

std::vector<Block*> Block::reachable_blocks(TraversalType traversal) {
  return traverse_generic<Block, true>(this, traversal);
}

std::vector<const Block*> Block::reachable_blocks(TraversalType traversal) const {
  return traverse_generic<const Block, true>(this, traversal);
}

std::unordered_set<Block*> Block::reachable_blocks_set(IncludeStart include_start) {
  // Prefer DFS as it's faster.
  return traverse_generic<Block, false>(this, include_start == IncludeStart::Yes
                                                ? TraversalType::DFS_WithStart
                                                : TraversalType::DFS_WithoutStart);
}

std::unordered_set<const Block*> Block::reachable_blocks_set(IncludeStart include_start) const {
  // Prefer DFS as it's faster.
  return traverse_generic<const Block, false>(this, include_start == IncludeStart::Yes
                                                      ? TraversalType::DFS_WithStart
                                                      : TraversalType::DFS_WithoutStart);
}
