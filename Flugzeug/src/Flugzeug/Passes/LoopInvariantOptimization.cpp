#include "LoopInvariantOptimization.hpp"

#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/Passes/Analysis/Loops.hpp>
#include <Flugzeug/Passes/Utils/LoopTransforms.hpp>
#include <Flugzeug/Passes/Utils/SimplifyPhi.hpp>

using namespace flugzeug;

static bool is_instruction_loop_invariant(Instruction* instruction,
                                          const analysis::Loop* loop,
                                          std::unordered_set<Instruction*>& invariants) {
  // Volatile instructions or loads cannot be loop invariants.
  if (instruction->is_volatile()) {
    return false;
  }

  switch (instruction->kind()) {
    case Value::Kind::Load:
    case Value::Kind::Phi:
      return false;

    default:
      break;
  }

  if (const auto phi = cast<Phi>(instruction)) {
    if (phi->block() != loop->header()) {
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
  stack.push_back(loop->header());

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
static bool optimize_invariants(Function* function, const analysis::Loop* loop) {
  const auto invariants = get_loop_invariants(function, loop);
  if (invariants.empty()) {
    return false;
  }

  // There needs to be a single block that jumps to this loop. We will get it or create it if there
  // are multiple entering blocks.
  const auto preheader = utils::get_or_create_loop_preheader(function, loop, true);

  // Move all invariants out of the loop.
  for (Instruction* invariant : invariants) {
    invariant->move_before(preheader->last_instruction());

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
  for (const auto& sub_loop : loop->sub_loops()) {
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
