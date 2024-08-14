#include "LoopRotation.hpp"
#include "Analysis/Loops.hpp"
#include "Utils/LoopTransforms.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static Block* get_actual_loop_body(Function* function, const analysis::Loop* loop) {
  const auto header = loop->get_header();
  const auto header_branch = cast<CondBranch>(header->last_instruction());
  if (!header_branch) {
    return nullptr;
  }

  bool exit_condition = false;
  {
    const auto true_target = header_branch->get_true_target();
    const auto false_target = header_branch->get_false_target();

    if (true_target == header || false_target == header) {
      return nullptr;
    }

    const auto is_true_exit = !loop->contains_block(true_target);
    const auto is_false_exit = !loop->contains_block(false_target);
    if (is_true_exit && !is_false_exit) {
      exit_condition = true;
    } else if (!is_true_exit && is_false_exit) {
      exit_condition = false;
    } else {
      return nullptr;
    }
  }

  return header_branch->get_target(!exit_condition);
}

static std::unordered_set<Instruction*> get_header_instructions_that_escape_loop(
  const analysis::Loop* loop,
  Block* exit_target) {
  std::unordered_set<Instruction*> escaping_instructions;

  // Go through every instruction in the loop header.
  for (Instruction& instruction : *loop->get_header()) {
    if (instruction.is_void()) {
      continue;
    }

    // Go through every instruction user to find if it's used outside the loop.
    const bool is_used_outside_loop =
      any_of(instruction.users<Instruction>(), [&](Instruction& user) {
        // If this instruction is used as incoming value in the Phi that is in the exit target block
        // then skip it - it's fine.
        if (user.block() == exit_target && cast<Phi>(user)) {
          return false;
        }

        return !loop->contains_block(user.block());
      });

    if (is_used_outside_loop) {
      escaping_instructions.insert(&instruction);
    }
  }

  return escaping_instructions;
}

static bool rotate_loop(Function* function, const analysis::Loop* loop) {
  // Change loop from form:
  //   while (n) {
  //     ...
  //   }
  // to:
  //   if (n) {
  //     while (n) {
  //       ...
  //     }
  //   }
  // In short we will do this by cloning the header block and making all back edges point to the
  // cloned block.

  if (loop->get_blocks().size() == 1) {
    return false;
  }

  const auto header = loop->get_header();

  const auto exit_target = loop->get_single_exit_target();
  if (!exit_target) {
    return false;
  }

  const auto actual_body = get_actual_loop_body(function, loop);
  if (!actual_body) {
    return false;
  }

  // If all blocks that contain back edge can also exit the loop it means that loop is in rotated
  // form.
  const auto all_back_edge_blocks_exit_loop =
    all_of(loop->get_back_edges_from(), [&](Block* block) {
      const auto cond_branch = cast<CondBranch>(block->last_instruction());
      if (!cond_branch) {
        return false;
      }
      return !loop->contains_block(cond_branch->get_true_target()) ||
             !loop->contains_block(cond_branch->get_false_target());
    });
  if (all_back_edge_blocks_exit_loop) {
    return false;
  }

  // Get all instructions that are created in the header and that have non-loop users. This
  // actually skips instructions that are only used in Phis in `exit_target`.
  const auto escaping_instructions = get_header_instructions_that_escape_loop(loop, exit_target);

  // We need single block that branches to the loop header. Create it if needed.
  const auto back_edge_block = utils::get_or_create_loop_single_back_edge_block(function, loop);

  std::vector<Phi*> phis;
  std::unordered_map<Instruction*, Instruction*> jump_back_mapping;

  // Clone the loop header as it contains the exit condition.
  const auto jump_back_block = function->create_block();
  {
    jump_back_mapping.reserve(header->instruction_count());

    for (Instruction& instruction : *header) {
      const auto cloned = instruction.clone();

      if (!instruction.is_void()) {
        jump_back_mapping.insert({&instruction, cloned});
      }

      // Update operands so they take cloned instructions instead of the original ones.
      // The only exceptions are Phi instructions.
      if (!cast<Phi>(instruction)) {
        cloned->transform_operands([&](Value* operand) -> Value* {
          const auto instruction = cast<Instruction>(operand);
          if (!instruction) {
            return nullptr;
          }

          const auto it = jump_back_mapping.find(instruction);
          if (it != jump_back_mapping.end()) {
            return it->second;
          }

          return nullptr;
        });
      }

      jump_back_block->push_instruction_back(cloned);
    }
  }

  // Check if block is part of the loop (accounting for newly created blocks that became part of the
  // loop too).
  const auto is_loop_block = [&](Block* block) {
    return loop->contains_block(block) || block == back_edge_block || block == jump_back_block;
  };

  // Make the back edge jump to the `jump_back_block` instead of the header.
  back_edge_block->last_instruction()->replace_operands(header, jump_back_block);

  // Merge values in the actual loop body.
  for (const auto& [header_instruction, jump_back_instruction] : jump_back_mapping) {
    // Fixup Phi instructions in header and jump back blocks.
    if (const auto header_phi = cast<Phi>(header_instruction)) {
      const auto jump_back_phi = cast<Phi>(jump_back_instruction);

      phis.push_back(header_phi);
      phis.push_back(jump_back_phi);

      // Back edge block doesn't branch to header anymore so remove incoming value for it.
      header_phi->remove_incoming(back_edge_block);

      // At this point nothing in the loop branches back to the header. Remove all non-loop incoming
      // values from jump-back block Phis.
      for (const auto header_predecessor : header->predecessors()) {
        jump_back_phi->remove_incoming(header_predecessor);
      }
    }

    // Merge values from header and jump-back blocks.
    // TODO: Check if we actually need it.
    const auto merging_phi =
      new Phi(function->context(),
              {{header, header_instruction}, {jump_back_block, jump_back_instruction}});
    actual_body->push_instruction_front(merging_phi);

    phis.push_back(merging_phi);

    // All the code so far uses `header_instruction`. The only places where it actually makes sense
    // are:
    //   1. Header block.
    //   2. Merging Phi instruction in the actual loop body.
    //   3. Blocks outside of the loop (these uses will be fixed up later).
    header_instruction->replace_uses_with_predicated(merging_phi, [&](User* user) -> bool {
      const auto instruction = cast<Instruction>(user);
      if (!instruction) {
        return false;
      }

      const auto block = instruction->block();

      return block != header && instruction != merging_phi && is_loop_block(block);
    });
  }

  // Update all Phi instructions in the exit target block. Previously the only loop block that
  // predecessed it was loop header. Now `jump_back_block` predecesses it too.
  for (Phi& phi : exit_target->instructions<Phi>()) {
    const auto incoming = phi.get_incoming_by_block(header);
    verify(incoming, "Phis in exit target block must contain header as incoming block");

    // Add incoming value for `jump_back_block`. If there is no mapping available that means that
    // header incoming value is global or that it was created in one of the header's dominators. In
    // this case we just add the original value.
    const auto it = jump_back_mapping.find(cast<Instruction>(incoming));
    phi.add_incoming(jump_back_block, it == jump_back_mapping.end() ? incoming : it->second);
  }

  // There are instructions that are part of the header block and they are used outside of the loop.
  // We need to replace all their uses outside of the loop with Phi created in the exit target
  // block.
  for (const auto header_value : escaping_instructions) {
    // Get actual loop value corresponding to the `header_value`.
    const auto it = jump_back_mapping.find(header_value);
    verify(it != jump_back_mapping.end(), "Failed to find value mapping");
    const auto loop_value = it->second;

    // Merge values from header and jump-back blocks.
    const auto merging_phi =
      new Phi(function->context(), {{header, header_value}, {jump_back_block, loop_value}});
    exit_target->push_instruction_front(merging_phi);

    // All users outside of the loop use value from header. Replace them with merging Phi.
    header_value->replace_uses_with_predicated(merging_phi, [&](User* user) -> bool {
      const auto instruction = cast<Instruction>(user);
      if (!instruction) {
        return false;
      }

      const auto block = instruction->block();

      // Don't replace uses inside the loop.
      if (is_loop_block(block)) {
        return false;
      }

      // Don't replace uses by Phi in `exit_target` block.
      if (block == exit_target && cast<Phi>(instruction)) {
        return false;
      }

      return true;
    });

    utils::simplify_phi(merging_phi, true);
  }

  // Try to simplify all created Phis.
  for (const auto phi : phis) {
    utils::simplify_phi(phi, true);
  }

  return true;
}

static bool rotate_loop_or_sub_loops(Function* function, const analysis::Loop* loop) {
  // Try rotating one of the sub-loops.
  bool rotated_subloop = false;
  for (const auto& sub_loop : loop->get_sub_loops()) {
    rotated_subloop |= rotate_loop_or_sub_loops(function, sub_loop.get());
  }

  if (rotated_subloop) {
    return true;
  }

  // If it didn't work then try rotating this loop.
  if (rotate_loop(function, loop)) {
    return true;
  }

  return false;
}

bool opt::LoopRotation::run(Function* function) {
  const auto loops = analysis::analyze_function_loops(function);

  bool did_something = false;

  for (const auto& loop : loops) {
    did_something |= rotate_loop_or_sub_loops(function, loop.get());
  }

  return did_something;
}