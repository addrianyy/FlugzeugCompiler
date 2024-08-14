#include "Inline.hpp"
#include "SimplifyPhi.hpp"

#include <Flugzeug/Core/Error.hpp>

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

class InlinedFunction {
  std::unordered_map<Value*, Value*> mapping;
  std::vector<Block*> blocks_;

 public:
  void add_block(Block* block) { blocks_.push_back(block); }
  const std::vector<Block*>& blocks() { return blocks_; }

  void add_mapping(Value* from, Value* to) { mapping.insert({from, to}); }

  template <typename T>
  T* map(T* value) {
    if (value->is_global()) {
      return value;
    }

    const auto it = mapping.find(value);
    if (it != mapping.end()) {
      return cast<T>(it->second);
    }

    fatal_error("No mapping for value {}.", value->format());
  }
};

/// Split block at `instruction`. `instruction` will be the last instruction of the old block.
static Block* split_block(Instruction* instruction) {
  const auto old_block = instruction->block();
  const auto new_block = old_block->function()->create_block();

  // Move all instructions after `instruction` to the new block.
  Instruction* current = instruction->next();
  verify(current, "Cannot split at the last instruction of the block");

  while (current) {
    const auto next = current->next();
    current->move_to_back(new_block);
    current = next;
  }

  // Update Phis that depend on `old_block`.
  for (Block* successor : new_block->successors()) {
    successor->replace_incoming_blocks_in_phis(old_block, new_block);
  }

  return new_block;
}

void flugzeug::utils::inline_call(Call* call) {
  const auto context = call->context();
  const auto caller = call->function();
  const auto callee = call->callee();

  verify(!callee->is_extern(), "Cannot inline external call");

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
  const auto return_type = callee->return_type();
  const auto return_phi = return_type->is_void() ? nullptr : new Phi(context, return_type);

  InlinedFunction inlined_function;

  // Create mapping from function parameter to passed argument.
  for (size_t i = 0; i < callee->parameter_count(); ++i) {
    const auto callee_value = callee->parameter(i);
    const auto caller_value = call->argument(i);

    inlined_function.add_mapping(callee_value, caller_value);
  }

  // Copy all blocks and instructions from callee to caller.
  for (Block& callee_block : *callee) {
    const auto caller_block = caller->create_block();

    inlined_function.add_mapping(&callee_block, caller_block);
    inlined_function.add_block(caller_block);

    for (Instruction& callee_instruction : callee_block) {
      Instruction* caller_instruction = callee_instruction.clone();

      if (!callee_instruction.is_void()) {
        inlined_function.add_mapping(&callee_instruction, caller_instruction);
      }

      caller_block->push_instruction_back(caller_instruction);
    }
  }

  // Fixup operands.
  for (Block* block : inlined_function.blocks()) {
    for (Instruction& instruction : advance_early(*block)) {
      // Replace returns with branches to `return_block`.
      if (const auto ret = cast<Ret>(instruction)) {
        // Add return value for this return site to Phi incoming values.
        if (return_phi) {
          return_phi->add_incoming(block, inlined_function.map(ret->return_value()));
        }

        ret->replace_with_instruction_and_destroy(new Branch(context, return_block));
        continue;
      }

      // Replace all operands from callee values to caller values.
      instruction.transform_operands([&](Value* operand) { return inlined_function.map(operand); });
    }
  }

  // Add branch to callee entry just before `call` instruction which will be removed soon.
  {
    const auto entry = inlined_function.blocks().front();
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