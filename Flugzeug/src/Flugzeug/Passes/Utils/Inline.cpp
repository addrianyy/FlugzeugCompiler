#include "Inline.hpp"
#include "SimplifyPhi.hpp"
#include <Flugzeug/Core/Error.hpp>
#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

/// Split block at `instruction`. `instruction` will be the last instruction of the old block.
static Block* split_block(Instruction* instruction) {
  const auto old_block = instruction->get_block();
  const auto new_block = old_block->get_function()->create_block();

  // Move all instructions after `instruction` to the new block.
  Instruction* current = instruction->get_next();
  verify(current, "Cannot split at the last instruction of the block");

  while (current) {
    const auto next = current->get_next();

    current->unlink();
    new_block->push_instruction_back(current);

    current = next;
  }

  // Update Phis that depend on `old_block`.
  for (Block* successor : new_block->get_successors()) {
    successor->replace_incoming_blocks_in_phis(old_block, new_block);
  }

  return new_block;
}

void flugzeug::utils::inline_call(Call* call) {
  const auto context = call->get_context();
  const auto caller = call->get_function();
  const auto callee = call->get_target();

  verify(!callee->is_extern(), "Cannot inline extern call.");

  // Split the block:
  //   block1:
  //      ...
  //      call void function(...)
  //      ...
  // to:
  //   block1:
  //      ...
  //      call void function(...)
  //   block2:
  //      ...
  const auto return_block = split_block(call);

  // Create Phi for getting inlined function return value.
  const auto return_type = callee->get_return_type();
  const auto return_phi = return_type->is_void() ? nullptr : new Phi(context, return_type);

  std::unordered_map<Value*, Value*> value_replacements;
  const auto get_replacement = [&value_replacements](Value* value) -> Value* {
    if (value->is_global()) {
      return value;
    }

    const auto it = value_replacements.find(value);
    verify(it != value_replacements.end(), "No replacement for value (?)");
    return it->second;
  };

  // Create mapping from function parameter to passed argument.
  for (size_t i = 0; i < callee->get_parameter_count(); ++i) {
    const auto callee_value = callee->get_parameter(i);
    const auto caller_value = call->get_arg(i);

    value_replacements.insert({callee_value, caller_value});
  }

  std::vector<Block*> created_blocks;

  // Copy all blocks and instructions from callee to caller.
  for (Block& callee_block : *callee) {
    const auto caller_block = caller->create_block();

    value_replacements.insert({&callee_block, caller_block});
    created_blocks.push_back(caller_block);

    for (Instruction& callee_instruction : callee_block) {
      Instruction* caller_instruction = callee_instruction.clone();

      if (!callee_instruction.is_void()) {
        value_replacements.insert({&callee_instruction, caller_instruction});
      }

      caller_block->push_instruction_back(caller_instruction);
    }
  }

  // Fixup operands.
  for (Block* block : created_blocks) {
    for (Instruction& instruction : dont_invalidate_current(*block)) {
      // Replace returns with branches to `return_block`.
      if (const auto ret = cast<Ret>(instruction)) {
        // Add return value for this return site to Phi incoming values.
        if (return_phi) {
          return_phi->add_incoming(block, get_replacement(ret->get_val()));
        }

        ret->replace_with_instruction_and_destroy(new Branch(context, return_block));
        continue;
      }

      // Replace all operands from callee values to caller values.
      for (size_t i = 0; i < instruction.get_operand_count(); ++i) {
        instruction.set_operand(i, get_replacement(instruction.get_operand(i)));
      }
    }
  }

  // Add branch to callee entry just before `call` instruction which will be removed soon.
  {
    const auto entry = created_blocks[0];
    const auto branch_to_entry = new Branch(context, entry);

    branch_to_entry->insert_before(call);
  }

  if (return_phi) {
    return_block->push_instruction_front(return_phi);
    call->replace_uses_with_and_destroy(return_phi);

    utils::simplify_phi(return_phi, true);
  } else {
    call->destroy();
  }
}