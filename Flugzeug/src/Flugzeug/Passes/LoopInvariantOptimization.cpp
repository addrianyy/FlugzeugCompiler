#include "LoopInvariantOptimization.hpp"

#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <Flugzeug/Passes/Analysis/Loops.hpp>
#include <Flugzeug/Passes/Utils/SimplifyPhi.hpp>

using namespace flugzeug;

static bool is_instruction_loop_invariant(Instruction* instruction, const analysis::Loop* loop,
                                          std::unordered_set<Instruction*>& invariants) {
  // Volatile instructions or loads cannot be loop invariants.
  if (instruction->is_volatile()) {
    return false;
  }

  switch (instruction->get_kind()) {
  case Value::Kind::Load:
  case Value::Kind::Phi:
    return false;

  default:
    break;
  }

  if (const auto phi = cast<Phi>(instruction)) {
    if (phi->get_block() != loop->get_header()) {
      return false;
    }

    // This Phi is a loop invariant only if it contains self-reference and all other blocks aren't
    // part of the loop.
    for (const auto incoming : *phi) {
      if (incoming.value != phi && loop->contains_block(incoming.block)) {
        return false;
      }
    }

    return true;
  }

  // This instruction is a loop invariant if all its operands are loop invariants too.
  for (Value& operand : instruction->operands()) {
    if (operand.is_undef() || cast<Constant>(operand) || cast<Parameter>(operand)) {
      continue;
    }

    const auto operand_instruction = cast<Instruction>(&operand);
    if (!invariants.contains(operand_instruction)) {
      return false;
    }
  }

  return true;
}

static std::vector<Instruction*> get_loop_invariants(Function* function,
                                                     const analysis::Loop* loop) {
  std::unordered_set<Instruction*> invariants_set;
  std::vector<Instruction*> invariants;

  std::unordered_set<Block*> visited;
  std::vector<Block*> stack;
  stack.push_back(loop->get_header());

  while (!stack.empty()) {
    Block* block = stack.back();
    stack.pop_back();

    if (visited.contains(block)) {
      continue;
    }

    visited.insert(block);

    for (Instruction& instruction : *block) {
      if (is_instruction_loop_invariant(&instruction, loop, invariants_set)) {
        invariants_set.insert(&instruction);
        invariants.push_back(&instruction);
      }
    }

    for (Block* successor : block->successors()) {
      if (!visited.contains(successor) && loop->contains_block(successor)) {
        stack.push_back(successor);
      }
    }
  }

  return invariants;
}

static std::unordered_set<Block*> get_entering_blocks(const analysis::Loop* loop) {
  std::unordered_set<Block*> entering_blocks;

  for (auto predecessor : loop->get_header()->predecessors()) {
    if (!loop->contains_block(predecessor)) {
      entering_blocks.insert(predecessor);
    }
  }

  return entering_blocks;
}

static Block* get_or_create_loop_preheader(Function* function, const analysis::Loop* loop) {
  const auto entering_blocks = get_entering_blocks(loop);

  if (entering_blocks.size() == 1) {
    return *entering_blocks.begin();
  } else {
    // This loop has multiple entry points. We will create a preheader that will become a new loop
    // entry point. Every entrering block will branch to it.
    const auto preheader = function->create_block();

    // Make preheader branch to the loop.
    preheader->push_instruction_back(new Branch(function->get_context(), loop->get_header()));

    // Make every entering block branch to the preheader instead of the loop header.
    for (Block* entering_block : entering_blocks) {
      entering_block->get_last_instruction()->replace_operands(loop->get_header(), preheader);
    }

    // Fixup Phis in the loop header.
    for (Phi& phi : loop->get_header()->instructions<Phi>()) {
      // Every Phi in the loop header needs corresponding Phi in the preheader.
      auto corresponding_phi = new Phi(function->get_context(), phi.get_type());
      preheader->push_instruction_front(corresponding_phi);

      // Copy incoming values for `entering_blocks` from Phi in the header to newly created Phi in
      // the preheader.
      for (auto incoming : phi) {
        if (entering_blocks.contains(incoming.block)) {
          corresponding_phi->add_incoming(incoming.block, incoming.value);
        }
      }

      // As loop header isn't predecessed by `entering_blocks` anymore we need to remove Phi
      // incoming values for them.
      for (Block* entering_block : entering_blocks) {
        phi.remove_incoming(entering_block);
      }

      phi.add_incoming(preheader, corresponding_phi);

      // Simplify newly created Phi if possible.
      utils::simplify_phi(corresponding_phi, true);
    }

    return preheader;
  }
}

static bool optimize_invariants(Function* function, const analysis::Loop* loop) {
  const auto invariants = get_loop_invariants(function, loop);
  if (invariants.empty()) {
    return false;
  }

  // There needs to be a single block that jumps to this loop. We will get it or create it if there
  // are multiple entering blocks.
  const auto header_predecessor = get_or_create_loop_preheader(function, loop);

  // Move all invariants out of the loop.
  for (Instruction* invariant : invariants) {
    invariant->move_before(header_predecessor->get_last_instruction());

    if (const auto phi = cast<Phi>(invariant)) {
      verify(utils::simplify_phi(phi, true), "Failed to simplify Phi");
    }
  }

  return true;
}

static bool optimize_invariants_in_loop_or_sub_loops(Function* function,
                                                     const analysis::Loop* loop) {
  // Try optimizing this loop.
  if (optimize_invariants(function, loop)) {
    return true;
  }

  // If it didn't work then try optimizing one of the sub-loops.
  for (const auto& sub_loop : loop->get_sub_loops()) {
    if (optimize_invariants_in_loop_or_sub_loops(function, sub_loop.get())) {
      return true;
    }
  }

  return false;
}

bool opt::LoopInvariantOptimization::run(Function* function) {
  const auto loops = analysis::analyze_function_loops(function);

  bool did_something = false;

  for (const auto& loop : loops) {
    did_something |= optimize_invariants_in_loop_or_sub_loops(function, loop.get());
  }

  return did_something;
}
