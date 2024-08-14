#include "LoopTransforms.hpp"
#include "SimplifyPhi.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/Passes/Analysis/Loops.hpp>

using namespace flugzeug;

static Block* add_intermediate_block_between_edges(const std::unordered_set<Block*>& from,
                                                   Block* to) {
  const auto function = to->get_function();

  // Create an intermediate block and make it branch to the `to` block.
  const auto intermediate_block = function->create_block();
  intermediate_block->push_instruction_back(new Branch(function->context(), to));

  // Make every `from` block branch to the `intermediate_block` instead of `to`.
  for (const auto from_block : from) {
    verify(from_block->get_last_instruction()->replace_operands(to, intermediate_block),
           "There is no edge from `from` to `to`");
  }

  // For every Phi in the `to` block:
  //   1. Create Phi in the intermediate block. Move all incoming values that correspond to removed
  //      edges to the newly created Phi.
  //   2. Add created Phi as incoming value to the Phi in the destination block.
  for (Phi& phi : to->instructions<Phi>()) {
    const auto corresponding_phi = new Phi(function->context(), phi.type());
    intermediate_block->push_instruction_front(corresponding_phi);

    for (const auto from_block : from) {
      const auto incoming = phi.remove_incoming(from_block);
      corresponding_phi->add_incoming(from_block, incoming);
    }

    phi.add_incoming(intermediate_block, corresponding_phi);

    utils::simplify_phi(corresponding_phi, true);
  }

  return intermediate_block;
}

flugzeug::Block* utils::get_or_create_loop_preheader(Function* function,
                                                     const analysis::Loop* loop,
                                                     bool allow_conditional) {
  std::unordered_set<Block*> entering_blocks;

  for (const auto predecessor : loop->get_header()->predecessors()) {
    if (!loop->contains_block(predecessor)) {
      entering_blocks.insert(predecessor);
    }
  }

  if (entering_blocks.size() == 1 && allow_conditional) {
    return *entering_blocks.begin();
  }

  return add_intermediate_block_between_edges(entering_blocks, loop->get_header());
}

Block* utils::get_or_create_loop_dedicated_exit(Function* function, const analysis::Loop* loop) {
  const auto exit_target = loop->get_single_exit_target();
  if (!exit_target) {
    return nullptr;
  }

  const bool is_already_dedicated_exit =
    all_of(exit_target->predecessors(), [&](Block* block) { return loop->contains_block(block); });
  if (is_already_dedicated_exit) {
    return exit_target;
  }

  // Get all loop blocks that branch to the exit target.
  std::unordered_set<Block*> exiting_blocks;
  for (const auto predecessor : exit_target->predecessors()) {
    if (loop->contains_block(predecessor)) {
      exiting_blocks.insert(predecessor);
    }
  }

  return add_intermediate_block_between_edges(exiting_blocks, exit_target);
}

Block* utils::get_or_create_loop_single_back_edge_block(Function* function,
                                                        const analysis::Loop* loop) {
  // If there is only one block that branches to the header then just return it.
  const auto back_edge_block = loop->get_single_back_edge();
  if (back_edge_block) {
    return back_edge_block;
  }

  return add_intermediate_block_between_edges(loop->get_back_edges_from(), loop->get_header());
}