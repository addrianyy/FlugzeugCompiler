#include "DeadCodeElimination.hpp"
#include <Core/Iterator.hpp>
#include <IR/Block.hpp>
#include <IR/Function.hpp>

using namespace flugzeug;

bool DeadCodeElimination::try_to_eliminate(Instruction* instruction,
                                           std::vector<Instruction*>& worklist) {
  if (instruction->is_void() || instruction->is_volatile() ||
      instruction->get_user_count_excluding_self() > 0) {
    return false;
  }

  for (size_t i = 0; i < instruction->get_operand_count(); ++i) {
    Value* operand = instruction->get_operand(i);
    instruction->set_operand(i, nullptr);

    if (instruction == operand || operand->get_user_count_excluding_self() > 0) {
      continue;
    }

    if (const auto other_inst = cast<Instruction>(operand)) {
      worklist.push_back(other_inst);
    }
  }

  instruction->destroy();

  return true;
}

bool DeadCodeElimination::run(Function* function) {
  std::vector<Instruction*> worklist;
  bool did_something = false;

  for (Block& block : *function) {
    for (Instruction& instruction : dont_invalidate_current(block)) {
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