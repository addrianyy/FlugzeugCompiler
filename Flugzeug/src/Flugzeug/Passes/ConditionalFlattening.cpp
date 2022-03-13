#include "ConditionalFlattening.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <array>

using namespace flugzeug;

constexpr size_t flattening_instruction_treshold = 4;

static bool is_instruction_movable(Instruction* instruction) {
  return !instruction->is_volatile() && !cast<Load>(instruction);
}

template <size_t N> static bool move_instructions(const std::array<Block*, N>& from, Block* to) {
  size_t total_instructions = 0;

  // Count all instructions in the `from` blocks and make sure that they all are movable.
  for (Block* from_block : from) {
    // Count all instructions except the last branch.
    total_instructions += from_block->get_instruction_count() - 1;

    for (Instruction& instruction : *from_block) {
      // Skip last instruction in the block (branch).
      if (&instruction == from_block->get_last_instruction()) {
        continue;
      }

      // Make sure that the instruction is movable.
      if (!is_instruction_movable(&instruction)) {
        return false;
      }
    }
  }

  if (total_instructions > flattening_instruction_treshold) {
    return false;
  }

  // Copy instructions from `from` blocks to the beginning of `to` block.
  for (Block* from_block : from) {
    // As we are moving to the beggining of `to` we need to iterate over instructions in reverse
    // order.
    for (Instruction& instruction : advance_early(reversed(*from_block))) {
      // Skip last instruction in the block (branch).
      if (&instruction == from_block->get_last_instruction()) {
        continue;
      }

      // All Phis must be simplificable (block has only one predecessor).
      if (const auto phi = cast<Phi>(instruction)) {
        verify(utils::simplify_phi(phi, false), "Failed to simplify Phi");
      }

      instruction.move_before(to->get_first_instruction());
    }

    from_block->clear();
  }

  return true;
}

static void rewrite_phis_to_selects(Value* condition, Block* block, Block* on_true,
                                    Block* on_false) {
  verify(on_true != on_false, "Invalid blocks passed");

  for (Phi& phi : advance_early(block->instructions<Phi>())) {
    const auto true_value = phi.get_incoming_by_block(on_true);
    const auto false_value = phi.get_incoming_by_block(on_false);
    verify(true_value && false_value, "Invalid Phi incoming values");

    phi.replace_with_instruction_and_destroy(
      new Select(block->get_context(), condition, true_value, false_value));
  }
}

struct SkewedConditional {
  Block* skew;
  Block* exit;
  Block* true_block;
  Block* false_block;
};

static bool flatten_block(Block* block) {
  const auto cond_branch = cast<CondBranch>(block->get_last_instruction());
  if (!cond_branch) {
    return false;
  }

  const auto on_true = cond_branch->get_true_target();
  const auto on_false = cond_branch->get_false_target();

  // Skip conditional branches with both labels equal and skip loops.
  if (on_true == on_false || on_true == block || on_false == block) {
    return false;
  }

  const auto on_true_exit = on_true->get_single_successor();
  const auto on_false_exit = on_false->get_single_successor();

  // Skewed case:
  //   A
  //  / \
  // B   |
  //  \ /
  //   D
  // A - `block`
  // B - `skew`
  // D - `exit`
  std::optional<SkewedConditional> skewed;
  if (on_false_exit == on_true) {
    skewed = SkewedConditional{
      .skew = on_false, .exit = on_true, .true_block = block, .false_block = on_false};
  } else if (on_true_exit == on_false) {
    skewed = SkewedConditional{
      .skew = on_true, .exit = on_false, .true_block = on_true, .false_block = block};
  }
  if (skewed) {
    // `skew` should be only reachable from `block` and `exit` should be
    // reachable from `block` and `skew`.
    if (skewed->skew->get_single_predecessor() != block ||
        skewed->exit->predecessors().size() != 2) {
      return false;
    }

    // Skip loops.
    if (skewed->skew == skewed->exit || skewed->skew == block || skewed->exit == block) {
      return false;
    }

    // Try to move every instruction from `skew` to `exit`.
    if (!move_instructions(std::array{skewed->skew}, skewed->exit)) {
      return false;
    }

    rewrite_phis_to_selects(cond_branch->get_cond(), skewed->exit, skewed->true_block,
                            skewed->false_block);

    // Make `block` branch directly to `exit`.
    block->get_last_instruction()->replace_with_instruction_and_destroy(
      new Branch(block->get_context(), skewed->exit));

    skewed->skew->destroy();

    return true;
  }

  // Symmetric case:
  //   A
  //  / \
  // B   C
  //  \ /
  //   D
  // A - `block`
  // B - `on_true`
  // C - `on_false`
  // D - `exit`

  // Get exit block if there is a common one. Otherwise, we cannot optimize
  // the branch out, so we continue.
  if (!on_true_exit || !on_false_exit || on_true_exit != on_false_exit) {
    return false;
  }

  const auto exit = on_true_exit;

  // Skip loops.
  if (exit == block || exit == on_true || exit == on_false) {
    return false;
  }

  // We cannot optimize this branch if `on_true` or `on_false` block is
  // reachable from somewhere else then from `block`.
  if (on_true->get_single_predecessor() != block || on_false->get_single_predecessor() != block) {
    return false;
  }

  // We cannot optimize this branch if `exit` can be reached from
  // somewhere else then `on_true` or `on_false`.
  if (exit->predecessors().size() != 2) {
    return false;
  }

  // Try to move every instruction from `on_true` and `on_false` to `exit`.
  if (!move_instructions(std::array{on_true, on_false}, exit)) {
    return false;
  }

  rewrite_phis_to_selects(cond_branch->get_cond(), exit, on_true, on_false);

  // Make `block` branch directly to `exit`.
  block->get_last_instruction()->replace_with_instruction_and_destroy(
    new Branch(block->get_context(), exit));

  on_true->destroy();
  on_false->destroy();

  return true;
}

bool opt::ConditionalFlattening::run(Function* function) {
  bool did_something = false;

  while (true) {
    bool did_something_this_iteration = false;

    for (Block& block : *function) {
      if (flatten_block(&block)) {
        did_something_this_iteration = true;
        did_something = true;
        break;
      }
    }

    if (!did_something_this_iteration) {
      break;
    }
  }

  return did_something;
}
