#include "BlockInvariantPropagation.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static std::optional<std::pair<Value*, Value*>> get_edge_invariant(Block* from, Block* to) {
  // We can get invariant value if `from` ends with conditional branch with condition being
  // a result from `cmp ne` or `cmp eq` instruction.

  // Get the conditional branch at the end of `from`.
  const auto cond_branch = cast<CondBranch>(from->get_last_instruction());
  if (!cond_branch) {
    return std::nullopt;
  }

  const auto on_true = cond_branch->get_true_target();
  const auto on_false = cond_branch->get_false_target();

  // Get compare instruction that is used as `cond_branch` condition.
  const auto cmp = cast<IntCompare>(cond_branch->get_cond());
  if (!cmp) {
    return std::nullopt;
  }

  const auto cmp_lhs = cmp->get_lhs();
  const auto cmp_rhs = cmp->get_rhs();

  // We cannot get invariant if `on_true` == `on_false` or `cmp_lhs` == `cmp_rhs`
  // (it's an unconditional branch then).
  if (cmp_lhs == cmp_rhs || on_true == on_false) {
    return std::nullopt;
  }

  // if (a == b) { block1 } else { block2 } => we know that in `block1` a == b.
  // if (a != b) { block1 } else { block2 } => we know that in `block2` a == b.
  if ((cmp->get_pred() == IntPredicate::Equal && on_true == to) ||
      (cmp->get_pred() == IntPredicate::NotEqual && on_false == to)) {
    const auto is_lhs_const = cast<Constant>(cmp_lhs);
    const auto is_rhs_const = cast<Constant>(cmp_rhs);

    // If both are constant we don't care about picking one.
    if (is_lhs_const && is_rhs_const) {
      return std::nullopt;
    }

    // Return substitution of non-const value with const value.
    if (is_lhs_const) {
      return std::pair{cmp_rhs, cmp_lhs};
    }
    if (is_rhs_const) {
      return std::pair{cmp_lhs, cmp_rhs};
    }
  }

  return std::nullopt;
}

bool opt::BlockInvariantPropagation::run(Function* function) {
  // Certain blocks have invariants and to be reached some condition must be true.
  // if (x == y) { block1 }
  // In this case in block1 it is known that x == y. If one of these is constant we will
  // optimize all accesses to non-constant value.

  bool did_something = false;

  // We need to traverse blocks in the DFS order.
  const auto blocks =
    function->get_entry_block()->get_reachable_blocks(TraversalType::DFS_WithStart);

  std::unordered_map<Block*, std::unordered_map<Value*, Value*>> block_invariants;
  std::vector<std::unordered_map<Value*, Value*>> predecessor_invariants;

  for (Block* block : blocks) {
    // Entry block doesn't have any invariants so nothing can be optimized.
    if (block->is_entry_block()) {
      continue;
    }

    // Reuse the previous allocations.
    const size_t predecessor_count = block->predecessors().size();
    if (predecessor_count > predecessor_invariants.size()) {
      predecessor_invariants.resize(predecessor_count);
    }

    // Get invariants for every predecessor.
    size_t predecessor_index = 0;
    for (Block* predecessor : block->predecessors()) {
      auto& current_invariants = predecessor_invariants[predecessor_index++];
      current_invariants.clear();

      // Get known invariants for this predecessor. If we don't know them just use an empty
      // invariant list.
      const auto it = block_invariants.find(predecessor);
      if (it != block_invariants.end()) {
        current_invariants = it->second;
      }

      // Get new invariant that we may know thanks to the conditional branch result. Try to insert
      // it into invariant list for the predecessor.
      const auto new_invariant = get_edge_invariant(predecessor, block);
      if (new_invariant) {
        if (current_invariants.erase(new_invariant->second) > 0) {
          // If there is a cyclic substiution (from, to) and (to, from) then remove it and don't
          // continue further.
        } else {
          const auto old_to = current_invariants.find(new_invariant->first);
          if (old_to != current_invariants.end()) {
            // If there is already invariant from `from` we will keep it only if
            // `to` values match.
            if (old_to->second != new_invariant->second) {
              current_invariants.erase(new_invariant->first);
            }
          } else {
            current_invariants.insert(*new_invariant);
          }
        }
      }
    }

    std::unordered_map<Value*, Value*> final_invariants;

    // Merge all invariants from predecessors into one invariant list.
    for (const auto [from, to] : predecessor_invariants[0]) {
      bool valid = true;

      // Make sure that this invariant is valid from every predecessor.
      for (size_t i = 1; i < predecessor_count; ++i) {
        const auto& other = predecessor_invariants[i];
        const auto from_it = other.find(from);

        // If this block has conflicting or not matching invariant then whole invariant is invalid.
        if (from_it == other.end() || from_it->second != to || other.contains(to)) {
          valid = false;
          break;
        }
      }

      if (valid) {
        final_invariants.insert({from, to});
      }
    }

    // Transform operands in the block based on calculated invariants.
    for (Instruction& instruction : *block) {
      did_something |= instruction.transform_operands([&](Value* operand) -> Value* {
        const auto it = final_invariants.find(operand);
        if (it != final_invariants.end()) {
          return it->second;
        }

        return nullptr;
      });
    }

    // Set computed invariants for this block.
    block_invariants.insert({block, std::move(final_invariants)});
  }

  return did_something;
}