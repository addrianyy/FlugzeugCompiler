#include "GlobalReordering.hpp"
#include "Analysis/Loops.hpp"
#include "Analysis/Paths.hpp"

#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static bool can_be_reordered(const Instruction* instruction) {
  if (instruction->is_volatile()) {
    return false;
  }

  switch (instruction->get_kind()) {
  case Value::Kind::Load:
  case Value::Kind::Phi:
    return false;

  default:
    return true;
  }
}

static bool do_users_allow_reordering(Instruction* instruction,
                                      const std::unordered_set<Block*>& loop_blocks) {
  for (Instruction& user : instruction->users<Instruction>()) {
    // We cannot reorder within the same block.
    if (user.get_block() == instruction->get_block()) {
      return false;
    }

    // We cannot reorder anything that is used as incoming value in Phi.
    if (cast<Phi>(user)) {
      return false;
    }

    // We cannot reorder anything that is in the loop as that would interfere with loop invariant
    // optimization.
    if (loop_blocks.contains(user.get_block())) {
      return false;
    }
  }

  return true;
}

Instruction* find_best_user(Instruction* instruction, const std::unordered_set<Instruction*>& users,
                            const DominatorTree& dominator_tree,
                            analysis::PathValidator& path_validator) {
  const auto check_user = [&](Instruction* best_user,
                              size_t instruction_count_limit) -> std::optional<size_t> {
    size_t instruction_count = 0;

    for (Instruction* user : users) {
      // We don't care about ourselves.
      if (user == best_user) {
        continue;
      }

      const auto result = path_validator.validate_path_count(dominator_tree, best_user, user);
      if (!result) {
        return std::nullopt;
      }

      instruction_count += *result;
      if (instruction_count >= instruction_count_limit) {
        return std::nullopt;
      }
    }

    return instruction_count;
  };

  Instruction* best_location = nullptr;
  size_t best_instruction_count = std::numeric_limits<size_t>::max();

  for (Instruction* best_user : users) {
    const auto instruction_count = check_user(best_user, best_instruction_count);
    if (instruction_count && *instruction_count < best_instruction_count) {
      best_instruction_count = *instruction_count;
      best_location = best_user;
    }
  }

  return best_location;
}

bool opt::GlobalReordering::run(Function* function) {
  // Reorder instructions so they are executed just before first instruction that needs them. This
  // reduces register pressure and makes IR easier to follow. Deduplication pass may create values
  // which are far away from first use. Limitation is that we can reorder things to a different
  // block then current one. This is because of a few reasons:
  // 1. It doesn't collide with local reordering pass.
  // 2. It avoids infinite loops.
  //    v10 = add v0, v1
  //    v11 = add v2, v3
  //    v12 = add v10, v11
  //    In this case v10 and v11 will be swapped each pass and optimization will never
  //    finish.

  analysis::PathValidator path_validator;
  const DominatorTree dominator_tree(function);

  // Find all blocks in the loops to not interfere with loop invariant optimization.
  std::unordered_set<Block*> loop_blocks;
  {
    const auto loops = analysis::analyze_function_loops(function, dominator_tree);
    for (const auto& loop : loops) {
      const auto& blocks = loop->get_blocks();

      loop_blocks.reserve(blocks.size());
      for (const auto& block : blocks) {
        loop_blocks.insert(block);
      }
    }
  }

  std::unordered_set<Instruction*> users;
  std::unordered_set<Instruction*> reordered;

  for (Instruction& instruction : advance_early(function->instructions())) {
    if (!can_be_reordered(&instruction) || reordered.contains(&instruction) ||
        loop_blocks.contains(instruction.get_block())) {
      continue;
    }

    if (!do_users_allow_reordering(&instruction, loop_blocks)) {
      continue;
    }

    // Create unique user list.
    users.clear();
    for (Instruction& user : instruction.users<Instruction>()) {
      users.insert(&user);
    }

    const auto best_user = find_best_user(&instruction, users, dominator_tree, path_validator);
    if (best_user) {
      instruction.move_before(best_user);
      reordered.insert(&instruction);
    }
  }

  return !reordered.empty();
}