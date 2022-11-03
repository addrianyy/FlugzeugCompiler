#include "ConditionalFlattening.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <array>

#include "Utils/SimplifyPhi.hpp"

using namespace flugzeug;

constexpr size_t flattening_instruction_treshold = 4;

static bool can_speculate_instruction(Instruction* instruction) {
  return !instruction->is_volatile() && !cast<Load>(instruction);
}

struct SkewedConditional {
  Block* skew;
  Block* exit;
  Block* true_block;
  Block* false_block;
};

template <size_t N>
static bool speculate_instructions(const std::array<Block*, N>& from, Block* to) {
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

      // All Phis will be simplified so they don't count here.
      if (cast<Phi>(&instruction)) {
        total_instructions--;
      }

      // Make sure that the instruction is movable.
      if (!can_speculate_instruction(&instruction)) {
        return false;
      }
    }
  }

  if (total_instructions > flattening_instruction_treshold) {
    return false;
  }

  // Copy instructions from `from` blocks to the end of `to` block.
  for (Block* from_block : from) {
    for (Instruction& instruction : advance_early(*from_block)) {
      // Skip last instruction in the block (branch).
      if (&instruction == from_block->get_last_instruction()) {
        continue;
      }

      // All Phis must be simplificable (block has only one predecessor).
      if (const auto phi = cast<Phi>(instruction)) {
        verify(utils::simplify_phi(phi, false), "Failed to simplify Phi");
        continue;
      }

      instruction.move_before(to->get_last_instruction());
    }

    from_block->clear();
  }

  return true;
}

template <size_t N>
static bool flatten(const std::array<Block*, N>& speculated_blocks,
                    CondBranch* cond_branch,
                    Block* true_block,
                    Block* false_block,
                    Block* exit) {
  const auto block = cond_branch->get_block();

  // All speculated blocks must have only `block` as predecessor.
  for (const auto speculated : speculated_blocks) {
    if (speculated->get_single_predecessor() != block) {
      return false;
    }
  }

  if (!speculate_instructions(speculated_blocks, block)) {
    return false;
  }

  for (auto& phi : exit->instructions<Phi>()) {
    const auto true_value = phi.remove_incoming(true_block);
    const auto false_value = phi.remove_incoming(false_block);

    const auto select =
      new Select(block->get_context(), cond_branch->get_cond(), true_value, false_value);
    select->insert_before(cond_branch);

    phi.add_incoming(block, select);
  }

  // Make `block` branch directly to `exit`.
  cond_branch->replace_with_instruction_and_destroy(new Branch(block->get_context(), exit));

  // Destroy speculated blocks.
  for (const auto speculated : speculated_blocks) {
    speculated->destroy();
  }

  return true;
}

static bool try_flatten_block(Block* block) {
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
  {
    std::optional<SkewedConditional> skewed;
    if (on_false_exit == on_true) {
      skewed = SkewedConditional{
        .skew = on_false,
        .exit = on_true,
        .true_block = block,
        .false_block = on_false,
      };
    } else if (on_true_exit == on_false) {
      skewed = SkewedConditional{
        .skew = on_true,
        .exit = on_false,
        .true_block = on_true,
        .false_block = block,
      };
    }
    if (skewed) {
      return flatten(std::array{skewed->skew}, cond_branch, skewed->true_block, skewed->false_block,
                     skewed->exit);
    }
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

  return flatten(std::array{on_true, on_false}, cond_branch, on_true, on_false, exit);
}

bool opt::ConditionalFlattening::run(Function* function) {
  bool did_something = false;

  while (true) {
    bool did_something_this_iteration = false;

    for (Block& block : *function) {
      if (try_flatten_block(&block)) {
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
