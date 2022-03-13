#include "LoopRotation.hpp"
#include "Analysis/Loops.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static Block* get_or_create_single_back_edge_block(Function* function, const analysis::Loop* loop) {
  const auto header = loop->get_header();

  {
    // If there is only one block that branches to the header then just return it.
    const auto back_edge_block = loop->get_single_back_edge();
    if (back_edge_block) {
      return back_edge_block;
    }
  }

  // We need to create additional block that will become our single back edge block.
  // We make it branch to the loop header.
  const auto back_edge_block = function->create_block();
  const auto branch_to_header = new Branch(function->get_context(), header);
  back_edge_block->push_instruction_back(branch_to_header);

  // Modify all branches to the header in the loop so they branch to the `back_edge_block` instead.
  for (const auto back_edge_from : loop->get_back_edges_from()) {
    back_edge_from->get_last_instruction()->replace_operands(header, back_edge_block);
  }

  // For every Phi in the header:
  //   1. Create Phi in the back edge block. Move all incoming values that correspond to previous
  //      back edge blocks to our new single back edge block.
  //   2. Add created Phi as incoming value to the Phi in the header.
  for (Phi& phi : header->instructions<Phi>()) {
    const auto corresponding_phi = new Phi(function->get_context(), phi.get_type());
    corresponding_phi->insert_before(branch_to_header);

    for (const auto back_edge_from : loop->get_back_edges_from()) {
      const auto incoming = phi.get_incoming_by_block(back_edge_from);
      verify(incoming, "Failed to get incoming value for back edge block");

      phi.remove_incoming(back_edge_from);
      corresponding_phi->add_incoming(back_edge_from, incoming);
    }

    phi.add_incoming(back_edge_block, corresponding_phi);

    utils::simplify_phi(corresponding_phi, true);
  }

  return back_edge_block;
}

static Block* get_actual_loop_body(Function* function, const analysis::Loop* loop) {
  const auto header = loop->get_header();
  const auto header_branch = cast<CondBranch>(header->get_last_instruction());
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

static std::unordered_set<Instruction*>
get_header_instructions_that_escape_loop(const analysis::Loop* loop, Block* exit_target) {
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
        if (user.get_block() == exit_target && cast<Phi>(user)) {
          return false;
        }

        return !loop->contains_block(user.get_block());
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
  //     ..
  //   }
  // to:
  //   if (n) {
  //     while (n) {
  //       ..
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

  // Get all instructions that are created in header and that have non-loop users. This actually
  // skips instructions that are only used in Phis in `exit_target`.
  const auto escaping_instructions = get_header_instructions_that_escape_loop(loop, exit_target);

  // We need single block that branches to the loop header. Create it if needed.
  const auto back_edge_block = get_or_create_single_back_edge_block(function, loop);

  std::vector<std::pair<Phi*, Phi*>> phis;
  std::unordered_map<Value*, Value*> jump_back_mapping;

  // Clone the loop header as it contains exit condition.
  const auto jump_back_block = function->create_block();
  {
    jump_back_mapping.reserve(header->get_instruction_count());

    for (Instruction& instruction : *header) {
      const auto cloned = instruction.clone();
      if (!instruction.is_void()) {
        jump_back_mapping.insert({&instruction, cloned});
      }

      // Save old and new Phi pairs as they will be needed later.
      if (const auto original_phi = cast<Phi>(instruction)) {
        phis.emplace_back(original_phi, cast<Phi>(cloned));
      }

      // Update operands so they take cloned instructions instead of the original ones.
      cloned->transform_operands([&](Value* operand) -> Value* {
        const auto it = jump_back_mapping.find(operand);
        if (it != jump_back_mapping.end()) {
          return it->second;
        }
        return nullptr;
      });

      jump_back_block->push_instruction_back(cloned);
    }
  }

  // Check if block is part of the loop (accounting for newly created blocks that became part of the
  // loop too).
  const auto is_loop_block = [&](Block* block) {
    return loop->contains_block(block) || block == back_edge_block || block == jump_back_block;
  };

  // Make back edge jump to the `jump_back_block` instead of the header.
  back_edge_block->get_last_instruction()->replace_operands(header, jump_back_block);

  // Fixup Phi instructions in header and jump back blocks.
  for (const auto& [header_phi, jump_back_phi] : phis) {
    // Back edge block doesn't branch to header anymore so remove incoming value for it.
    header_phi->remove_incoming(back_edge_block);

    // At this point nothing in the loop branches back to the header. Remove all non-loop incoming
    // values from jump-back block Phis.
    for (const auto header_predecessor : header->predecessors()) {
      jump_back_phi->remove_incoming(header_predecessor);
    }

    // Merge values from header and jump-back blocks.
    const auto merging_phi =
      new Phi(function->get_context(), {{header, header_phi}, {jump_back_block, jump_back_phi}});
    actual_body->push_instruction_front(merging_phi);

    // All the code so far uses `header_phi`. The only places where it actually makes sense are:
    //   1. Header block.
    //   2. Merging Phi instruction in the actual loop body.
    //   3. Blocks outside of the loop (these uses will be fixed up later).
    header_phi->replace_uses_with_predicated(merging_phi, [&](User* user) -> bool {
      const auto instruction = cast<Instruction>(user);
      if (!instruction) {
        return false;
      }

      const auto block = instruction->get_block();

      return block != header && instruction != merging_phi && is_loop_block(block);
    });
  }

  // Update all Phi instructions in the exit target block. Previously the only loop block that
  // predecessed it was loop header. Now `jump_back_block` predecesses it too.
  for (Phi& phi : exit_target->instructions<Phi>()) {
    const auto incoming = phi.get_incoming_by_block(header);
    verify(incoming, "Phis in exit target block must contain entry as incoming block");

    // Add incoming value for `jump_back_block`. If there is no mapping available that means that
    // header incoming value is global or that it was created in one of the header's dominators. In
    // this case we just add the original value.
    const auto it = jump_back_mapping.find(incoming);
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
      new Phi(function->get_context(), {{header, header_value}, {jump_back_block, loop_value}});
    exit_target->push_instruction_front(merging_phi);

    // All users outside of the loop use value from header. Replace them with merging Phi.
    header_value->replace_uses_with_predicated(merging_phi, [&](User* user) -> bool {
      const auto instruction = cast<Instruction>(user);
      if (!instruction) {
        return false;
      }

      const auto block = instruction->get_block();

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

  // Try to simplify all Phis in header and jump back blocks.
  for (const auto& [header_phi, jump_back_phi] : phis) {
    utils::simplify_phi(header_phi, true);
    utils::simplify_phi(jump_back_phi, true);
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