#include "DeadCodeElimination.hpp"

#include <Flugzeug/Core/Iterator.hpp>

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>

using namespace flugzeug;

static bool try_to_eliminate(Instruction* instruction, std::vector<Instruction*>& worklist) {
  // Skip instructions which cannot be eliminated.
  if (instruction->is_void() || instruction->is_volatile() ||
      instruction->get_user_count_excluding_self() > 0) {
    return false;
  }

  for (size_t i = 0; i < instruction->get_operand_count(); ++i) {
    Value* operand = instruction->get_operand(i);
    instruction->set_operand(i, nullptr);

    // Check if by removing instruction operand we have made operand dead too.
    if (const auto operand_instruction = cast<Instruction>(operand)) {
      if (instruction == operand || operand->get_user_count_excluding_self() > 0) {
        continue;
      }

      worklist.push_back(operand_instruction);
    }
  }

  instruction->destroy();
  return true;
}

bool DeadCodeElimination::run(Function* function) {
  std::vector<Instruction*> worklist;
  bool did_something = false;

  for (Instruction& instruction : dont_invalidate_current(function->instructions())) {
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