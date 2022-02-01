#include "LoopUnrolling.hpp"

#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <Flugzeug/Passes/Analysis/Loops.hpp>
#include <Flugzeug/Passes/Utils/Evaluation.hpp>

#include <Flugzeug/Core/Log.hpp>

using namespace flugzeug;

struct LoopPhi {
  Value* first_value;
  Value* previous_iteration_value;
};

static bool get_loop_count_related_instructions(Instruction* instruction,
                                                std::unordered_set<Instruction*>& instructions,
                                                std::unordered_map<Phi*, LoopPhi>& loop_phis,
                                                const Loop* loop, const Block* back_edge_from,
                                                const DominatorTree& dominator_tree) {
  // Return if we have already seen this instruction.
  if (instructions.contains(instruction)) {
    return true;
  }

  if (const auto phi = cast<Phi>(instruction)) {
    // Phis related to loop count can be only placed in the loop header.
    if (phi->get_block() != loop->header) {
      return false;
    }

    const auto previous_iteration_value = phi->get_incoming_by_block(back_edge_from);
    Value* common_other_value = nullptr;

    for (const auto& incoming : *phi) {
      if (incoming.block == back_edge_from) {
        continue;
      }

      if (!common_other_value) {
        common_other_value = incoming.value;
      }

      if (common_other_value != incoming.value) {
        return false;
      }
    }

    if (!common_other_value || !common_other_value->is_global()) {
      return false;
    }

    instructions.insert(instruction);

    LoopPhi loop_phi{};
    loop_phi.previous_iteration_value = previous_iteration_value;
    loop_phi.first_value = common_other_value;
    loop_phis[phi] = loop_phi;

    if (const auto prev_iteration_instruction = cast<Instruction>(previous_iteration_value)) {
      return get_loop_count_related_instructions(prev_iteration_instruction, instructions,
                                                 loop_phis, loop, back_edge_from, dominator_tree);
    } else {
      return true;
    }
  }

  // We can easily interpret only BinaryInstr/UnaryInstr/Cast.
  if (!cast<BinaryInstr>(instruction) && !cast<UnaryInstr>(instruction) &&
      !cast<Cast>(instruction)) {
    return false;
  }

  // This instruction must be part of the loop.
  if (!loop->blocks_without_subloops.contains(instruction->get_block())) {
    return false;
  }

  // This instruction must not be executed conditionally in the loop.
  if (!instruction->get_block()->dominates(back_edge_from, dominator_tree)) {
    return false;
  }

  instructions.insert(instruction);

  for (Value& operand : instruction->operands()) {
    if (const auto operand_instruction = cast<Instruction>(operand)) {
      if (!get_loop_count_related_instructions(operand_instruction, instructions, loop_phis, loop,
                                               back_edge_from, dominator_tree)) {
        return false;
      }
    }
  }

  return true;
}

static std::vector<Instruction*>
order_loop_count_related_instructions(const Loop* loop,
                                      const std::unordered_set<Instruction*>& instruction_set) {
  std::vector<Instruction*> instructions;

  std::unordered_set<Block*> visited;
  std::vector<Block*> stack;
  stack.push_back(loop->header);

  while (!stack.empty()) {
    Block* block = stack.back();
    stack.pop_back();

    if (visited.contains(block)) {
      continue;
    }

    visited.insert(block);

    for (Instruction& instruction : *block) {
      if (instruction_set.contains(&instruction)) {
        instructions.push_back(&instruction);
      }
    }

    for (Block* successor : block->get_successors()) {
      if (!visited.contains(successor) && loop->blocks.contains(successor)) {
        stack.push_back(successor);
      }
    }
  }

  verify(instructions.size() == instruction_set.size(), "Ordered instructions size mismatch");

  return instructions;
}

static std::optional<size_t> get_unroll_count(const std::vector<Instruction*>& instructions,
                                              const std::unordered_map<Phi*, LoopPhi>& loop_phis,
                                              bool condition_to_continue) {
  std::unordered_map<Value*, uint64_t> values[2];
  size_t current_values_index = 0;

  const auto get_constant_custom = [&](const std::unordered_map<Value*, uint64_t>& value_map,
                                       Value* value, uint64_t& constant) -> bool {
    if (const auto constant_value = cast<Constant>(value)) {
      constant = constant_value->get_constant_u();
      return true;
    }

    if (value->is_undef()) {
      constant = 0;
      return true;
    }

    const auto it = value_map.find(value);
    if (it != value_map.end()) {
      constant = it->second;
      return true;
    }

    return false;
  };

  const auto get_constant = [&](Value* value, uint64_t& constant) -> bool {
    return get_constant_custom(values[current_values_index], value, constant);
  };

  const auto get_constant_previous = [&](Value* value, uint64_t& constant) -> bool {
    return get_constant_custom(values[(current_values_index + 1) % 2], value, constant);
  };

  const auto set_value = [&](Value* value, uint64_t constant) {
    values[current_values_index][value] = constant;
  };

  for (size_t iteration = 0; iteration < 12; ++iteration) {
    for (Instruction* instruction : instructions) {
      if (auto unary_instr = cast<UnaryInstr>(instruction)) {
        uint64_t constant;
        if (!get_constant(unary_instr->get_val(), constant)) {
          return std::nullopt;
        }
        set_value(instruction, utils::evaluate_unary_instr(unary_instr->get_type(),
                                                           unary_instr->get_op(), constant));
      } else if (auto binary_instr = cast<BinaryInstr>(instruction)) {
        uint64_t lhs, rhs;
        if (!get_constant(binary_instr->get_lhs(), lhs) ||
            !get_constant(binary_instr->get_rhs(), rhs)) {
          return std::nullopt;
        }
        set_value(instruction, utils::evaluate_binary_instr(binary_instr->get_type(), lhs,
                                                            binary_instr->get_op(), rhs));
      } else if (auto cast_instr = cast<Cast>(instruction)) {
        uint64_t constant;
        if (!get_constant(cast_instr->get_val(), constant)) {
          return std::nullopt;
        }
        set_value(instruction,
                  utils::evaluate_cast(constant, cast_instr->get_val()->get_type(),
                                       cast_instr->get_type(), cast_instr->get_cast_kind()));
      } else if (auto phi = cast<Phi>(instruction)) {
        const auto& loop_phi = loop_phis.find(phi)->second;

        Value* value = iteration == 0 ? loop_phi.first_value : loop_phi.previous_iteration_value;
        uint64_t constant;
        if (!get_constant_previous(value, constant)) {
          return std::nullopt;
        }
        set_value(instruction, constant);
      } else if (auto cmp = cast<IntCompare>(instruction)) {
        uint64_t lhs, rhs;
        if (!get_constant(cmp->get_lhs(), lhs) || !get_constant(cmp->get_rhs(), rhs)) {
          return std::nullopt;
        }

        const auto result =
          utils::evaluate_int_compare(cmp->get_lhs()->get_type(), lhs, cmp->get_pred(), rhs);
        if (result != condition_to_continue) {
          return iteration + 1;
        }
      } else {
        fatal_error("Encountered unexpected instruction when interpreting the loop.");
      }
    }

    current_values_index = (current_values_index + 1) % 2;
  }

  return std::nullopt;
}

static bool do_unroll(Function* function, const Loop* loop, size_t unroll_count) { return false; }

static bool try_unroll_loop(Function* function, const Loop* loop,
                            const DominatorTree& dominator_tree) {
  if (loop->exiting_edges.size() != 1 || loop->back_edges_from.size() != 1) {
    return false;
  }

  const auto [exit_from, exit_to] = loop->exiting_edges[0];
  const auto back_edge_from = *loop->back_edges_from.begin();

  const auto exit_instruction = cast<CondBranch>(exit_from->get_last_instruction());
  if (!exit_instruction) {
    return false;
  }
  const auto exit_condition = cast<IntCompare>(exit_instruction->get_cond());
  if (!exit_condition) {
    return false;
  }

  bool condition_to_continue = false;
  if (exit_instruction->get_true_target() == exit_to &&
      loop->blocks_without_subloops.contains(exit_instruction->get_false_target())) {
    condition_to_continue = false;
  } else if (exit_instruction->get_false_target() == exit_to &&
             loop->blocks_without_subloops.contains(exit_instruction->get_true_target())) {
    condition_to_continue = true;
  } else {
    return false;
  }

  std::unordered_set<Instruction*> instruction_set;
  std::unordered_map<Phi*, LoopPhi> loop_phis;
  instruction_set.insert(exit_condition);

  for (Value& operand : exit_condition->operands()) {
    if (const auto instruction = cast<Instruction>(operand)) {
      if (!get_loop_count_related_instructions(instruction, instruction_set, loop_phis, loop,
                                               back_edge_from, dominator_tree)) {
        return false;
      }
    }
  }

  const auto instructions = order_loop_count_related_instructions(loop, instruction_set);
  const auto unroll_count = get_unroll_count(instructions, loop_phis, condition_to_continue);

  if (true) {
    log_debug("Function {}, loop {}: loop condition: {} ({} to continue)", function->get_name(),
              loop->header->format(), exit_condition->format(), condition_to_continue);
    if (unroll_count) {
      log_debug("Unroll count: {}", *unroll_count);
    } else {
      log_debug("Unroll count: unknown");
    }
    log_debug("Instructions related to loop count:");
    for (auto& instruction : instructions) {
      instruction->print();
    }
    log_debug("");
  }

  if (unroll_count) {
    return do_unroll(function, loop, *unroll_count);
  }

  return false;
}

static bool unroll_loop(Function* function, const Loop* loop, const DominatorTree& dominator_tree) {
  if (try_unroll_loop(function, loop, dominator_tree)) {
    return true;
  }

  for (const auto& sub_loop : loop->sub_loops) {
    if (unroll_loop(function, sub_loop.get(), dominator_tree)) {
      return true;
    }
  }

  return false;
}

bool LoopUnrolling::run(Function* function) {
  DominatorTree dominator_tree(function);

  const auto loops = analyze_function_loops(function);

  bool did_something = false;

  for (const auto& loop : loops) {
    did_something |= unroll_loop(function, loop.get(), dominator_tree);
  }

  return did_something;
}