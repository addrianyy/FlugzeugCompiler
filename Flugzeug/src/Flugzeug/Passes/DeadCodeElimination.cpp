#include "DeadCodeElimination.hpp"

#include <Flugzeug/Core/Iterator.hpp>

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static bool is_dead_recursive_phi(Instruction* instruction) {
  if (const auto phi = cast<Phi>(instruction)) {
    // If all users of this Phi instruction are non-volatile and are used only by this Phi
    // instruction we can safely remove it.
    // Note that this will only handle the simplest cases.
    return all_of(phi->users<Instruction>(), [&phi](Instruction& user) -> bool {
      return !user.is_volatile() && user.is_used_only_by(phi);
    });
  } else {
    return false;
  }
}

static bool try_to_eliminate(Instruction* instruction, std::vector<Instruction*>& worklist) {
  // Skip instructions which cannot be eliminated.
  if (!is_dead_recursive_phi(instruction) &&
      (instruction->is_used() || instruction->is_void() || instruction->is_volatile())) {
    return false;
  }

  for (size_t i = 0; i < instruction->get_operand_count(); ++i) {
    Value* operand = instruction->get_operand(i);
    instruction->set_operand(i, nullptr);

    // Check if by removing instruction operand we have made operand dead too.
    if (const auto operand_instruction = cast<Instruction>(operand)) {
      if (instruction == operand || operand->is_used()) {
        continue;
      }

      worklist.push_back(operand_instruction);
    }
  }

  instruction->destroy();
  return true;
}

bool opt::DeadCodeElimination::run(Function* function) {
  std::vector<Instruction*> worklist;
  bool did_something = false;

  for (Instruction& instruction : advance_early(function->instructions())) {
    // Don't try to eliminate instructions that are already queued in worklist.
    const auto found = std::find(worklist.begin(), worklist.end(), &instruction);
    if (found == worklist.end()) {
      did_something |= try_to_eliminate(&instruction, worklist);
    }
  }

  while (!worklist.empty()) {
    Instruction* instruction = worklist.back();
    worklist.pop_back();

    did_something |= try_to_eliminate(instruction, worklist);
  }

  return did_something;
}