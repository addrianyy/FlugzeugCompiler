#include "LoopUnrolling.hpp"

#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/Passes/Analysis/Loops.hpp>
#include <Flugzeug/Passes/Utils/Evaluation.hpp>
#include <Flugzeug/Passes/Utils/SimplifyPhi.hpp>

using namespace flugzeug;

constexpr size_t loop_unrolling_max_iteration_count = 12;

class UnrolledIteration {
  std::unordered_map<Value*, Value*> mapping;
  std::unordered_map<Value*, Value*> reverse_mapping;
  std::vector<Block*> blocks;

 public:
  void add_block(Block* block) { blocks.push_back(block); }
  const std::vector<Block*>& get_blocks() { return blocks; }

  void reserve_mappings(size_t count) {
    mapping.reserve(count);
    reverse_mapping.reserve(count);
  }

  void add_mapping(Value* from, Value* to) {
    mapping.insert({from, to});
    reverse_mapping.insert({to, from});
  }

  template <typename T>
  T* map(T* value) {
    const auto it = mapping.find(value);
    if (it != mapping.end()) {
      return cast<T>(it->second);
    }

    return nullptr;
  }

  template <typename T>
  T* reverse_map(T* value) {
    const auto it = reverse_mapping.find(value);
    if (it != reverse_mapping.end()) {
      return cast<T>(it->second);
    }

    return nullptr;
  }

  void replace_value(Value* previous_value, Value* new_value) {
    const auto original_value = reverse_mapping.find(previous_value)->second;

    verify(mapping[original_value] == previous_value, "Reverse mapping is incorrect (?)");

    // Change `previous_value` to `new_value` in normal mapping.
    mapping[original_value] = new_value;

    // Change `previous_value` to `new_value` in reverse mapping.
    reverse_mapping.erase(previous_value);
    reverse_mapping.insert({new_value, original_value});
  }
};

struct LoopPhi {
  Value* first_iteration_value;
  Value* previous_iteration_value;
};

static bool get_loop_count_related_instructions(Instruction* instruction,
                                                std::unordered_set<Instruction*>& instructions,
                                                std::unordered_map<Phi*, LoopPhi>& loop_phis,
                                                const analysis::Loop* loop,
                                                const Block* back_edge_from,
                                                const DominatorTree& dominator_tree) {
  // Return if we have already seen this instruction.
  if (instructions.contains(instruction)) {
    return true;
  }

  // Phis require special handling as they are used to copy values from previous iteration to the
  // current one.
  if (const auto phi = cast<Phi>(instruction)) {
    // Because we are copying from previous iteration to current one, Phis must be placed in the
    // loop header (as this is where the back edge jumps to).
    if (phi->get_block() != loop->get_header()) {
      return false;
    }

    // Phi must have 2 values:
    //   1. previous iteration value
    //   2. first iteration value (there can be multiple incoming entries for it, but they
    //      all must have the same value)

    const auto previous_iteration_value = phi->get_incoming_by_block(back_edge_from);

    Value* first_iteration_value = nullptr;

    // Get common value for the first iteration.
    for (const auto& incoming : *phi) {
      if (incoming.block == back_edge_from) {
        continue;
      }

      if (!first_iteration_value) {
        first_iteration_value = incoming.value;
      }

      if (first_iteration_value != incoming.value) {
        return false;
      }
    }

    // If we haven't found a common first iteration value, or it's not a constant (or undef) then we
    // cannot unroll this loop.
    if (!first_iteration_value || !first_iteration_value->is_global()) {
      return false;
    }

    // This Phi is loop count related so add it to the set.
    instructions.insert(instruction);

    // Save information about this Phi as it will be used by the loop interpreter.
    loop_phis[phi] = LoopPhi{
      .first_iteration_value = first_iteration_value,
      .previous_iteration_value = previous_iteration_value,
    };

    // If previous iteration value comes from the instruction we need to process it aswell.
    if (const auto prev_iteration_instruction = cast<Instruction>(previous_iteration_value)) {
      return get_loop_count_related_instructions(prev_iteration_instruction, instructions,
                                                 loop_phis, loop, back_edge_from, dominator_tree);
    } else {
      return true;
    }
  }

  // We can interpret only BinaryInstr/UnaryInstr/Cast.
  if (!cast<BinaryInstr>(instruction) && !cast<UnaryInstr>(instruction) &&
      !cast<Cast>(instruction)) {
    return false;
  }

  // This instruction must be part of the loop.
  if (!loop->contains_block_skipping_sub_loops(instruction->get_block())) {
    return false;
  }

  // This instruction must be executed unconditionally in the loop.
  if (!instruction->get_block()->dominates(back_edge_from, dominator_tree)) {
    return false;
  }

  instructions.insert(instruction);

  // If this instruction is loop count related then instruction operands of it must be too. Process
  // them.
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

static std::vector<Instruction*> order_loop_count_related_instructions(
  const analysis::Loop* loop,
  const std::unordered_set<Instruction*>& instruction_set) {
  std::vector<Instruction*> instructions;

  std::unordered_set<Block*> visited;
  std::vector<Block*> stack;
  stack.push_back(loop->get_header());

  // Do DFS traversal on loop blocks to properly order loop count related instructions.
  while (!stack.empty()) {
    Block* block = stack.back();
    stack.pop_back();

    // Ignore already seen blocks.
    if (visited.contains(block)) {
      continue;
    }

    visited.insert(block);

    // Add instructions that we are interested in with proper order.
    for (Instruction& instruction : *block) {
      if (instruction_set.contains(&instruction)) {
        instructions.push_back(&instruction);
      }
    }

    for (Block* successor : block->successors()) {
      // We are only interested in blocks that are part of our loop.
      if (!visited.contains(successor) && loop->contains_block(successor)) {
        stack.push_back(successor);
      }
    }
  }

  // Make sure that DFS traversal didn't skip any instruction.
  verify(instructions.size() == instruction_set.size(), "Ordered instructions size mismatch");

  return instructions;
}

static std::optional<size_t> get_unroll_count(const std::vector<Instruction*>& instructions,
                                              const std::unordered_map<Phi*, LoopPhi>& loop_phis,
                                              bool condition_to_continue) {
  // Because Phis are executed simultaneously we need to have two maps (one for previous iteration
  // and one for current iteration). This is to avoid overwriting values to early which would be
  // incorrect for self-referential Phis.
  std::unordered_map<Value*, uint64_t> values[2];
  size_t current_values_index = 0;

  const auto get_constant_from_map = [&](const std::unordered_map<Value*, uint64_t>& value_map,
                                         Value* value, uint64_t& constant) -> bool {
    if (const auto constant_value = cast<Constant>(value)) {
      constant = constant_value->get_constant_u();
      return true;
    }

    // Undefined values are undetermined so for simplicity we can say that they are equal to 0.
    if (value->is_undef()) {
      constant = 0;
      return true;
    }

    // It's not a constant or undef, so we need to consult value map to get the value.
    const auto it = value_map.find(value);
    if (it != value_map.end()) {
      constant = it->second;
      return true;
    }

    return false;
  };

  // Get constant at current/previous iteration.
  const auto get_constant = [&](Value* value, uint64_t& constant) -> bool {
    return get_constant_from_map(values[current_values_index], value, constant);
  };
  const auto get_constant_in_previous_iteration = [&](Value* value, uint64_t& constant) -> bool {
    return get_constant_from_map(values[(current_values_index + 1) % 2], value, constant);
  };

  // Set constant at current iteration.
  const auto set_constant = [&](Value* value, uint64_t constant) {
    values[current_values_index][value] = constant;
  };

  // Try interpreting the loop for as many as `loop_unrolling_max_iteration_count` times to see
  // if we hit the exit condition. If we do we will know how many times we should unroll this loop.
  for (size_t iteration = 0; iteration < loop_unrolling_max_iteration_count; ++iteration) {
    for (Instruction* instruction : instructions) {
      if (auto unary_instr = cast<UnaryInstr>(instruction)) {
        uint64_t constant;
        if (!get_constant(unary_instr->get_val(), constant)) {
          return std::nullopt;
        }
        set_constant(instruction, utils::evaluate_unary_instr(unary_instr->get_type(),
                                                              unary_instr->get_op(), constant));
      } else if (auto binary_instr = cast<BinaryInstr>(instruction)) {
        uint64_t lhs, rhs;
        if (!get_constant(binary_instr->get_lhs(), lhs) ||
            !get_constant(binary_instr->get_rhs(), rhs)) {
          return std::nullopt;
        }
        set_constant(instruction, utils::evaluate_binary_instr(binary_instr->get_type(), lhs,
                                                               binary_instr->get_op(), rhs));
      } else if (auto cast_instr = cast<Cast>(instruction)) {
        uint64_t constant;
        if (!get_constant(cast_instr->get_val(), constant)) {
          return std::nullopt;
        }
        set_constant(instruction,
                     utils::evaluate_cast(constant, cast_instr->get_val()->get_type(),
                                          cast_instr->get_type(), cast_instr->get_cast_kind()));
      } else if (auto phi = cast<Phi>(instruction)) {
        const auto& loop_phi = loop_phis.find(phi)->second;

        // If this is the first iteration we will take `first_iteration_value`. If it's not we will
        // get `previous_iteration_value` constant from previous iteration.
        Value* value =
          iteration == 0 ? loop_phi.first_iteration_value : loop_phi.previous_iteration_value;
        uint64_t constant;
        if (!get_constant_in_previous_iteration(value, constant)) {
          return std::nullopt;
        }
        set_constant(instruction, constant);
      } else if (auto cmp = cast<IntCompare>(instruction)) {
        uint64_t lhs, rhs;
        if (!get_constant(cmp->get_lhs(), lhs) || !get_constant(cmp->get_rhs(), rhs)) {
          return std::nullopt;
        }

        const auto result =
          utils::evaluate_int_compare(cmp->get_lhs()->get_type(), lhs, cmp->get_pred(), rhs);

        // Check if we hit the exit condition.
        if (result != condition_to_continue) {
          return iteration + 1;
        }
      } else {
        fatal_error("Encountered unexpected instruction when interpreting the loop.");
      }
    }

    // Swap value maps.
    current_values_index = (current_values_index + 1) % 2;
  }

  return std::nullopt;
}

static void replace_branch(Instruction* instruction, Block* old_target, Block* new_target) {
  if (instruction->is_branching()) {
    instruction->replace_operands(old_target, new_target);
  }
}

static void perform_unrolling(Function* function,
                              const analysis::Loop* loop,
                              Block* exit_from,
                              Block* exit_to,
                              Block* back_edge_from,
                              size_t unroll_count) {
  // At this point we know that this loop is unrollable and we know how many times it should be
  // unrolled. Unrolling process itself looks like that:
  //   1. Create a new exit block. Make `exit_from` branch to the new exit block instead of
  //      `exit_to`. This new block will end with branch to `exit_to`. It will be used to store Phi
  //      instructions for values that escape the loop.
  //   2. Go through every value created in the body of the loop and check if it is used outside of
  //      it. If it is then create Phi instruction for it in the new exit block. This Phi will
  //      have incoming value for every unrolled exiting block.
  //   3. Replace all outside uses of loop escaping values with newly created Phi instructions.
  //   4. Copy all loop blocks `unroll_count` - 1 times.
  //   5. For every loop escaping value in every unrolled instance we add it to its corresponding
  //      Phi instruction incoming values.
  //   6. Replace Phis in the loop headers with values from previously unrolled iteration.
  //   7. Replace every back edge with branch to the next unrolled iteration (except for the last
  //      iteration).
  //   8. Back edge in the last unrolled iteration is unreachable. If it is conditional branch then
  //      we will make it unconditional so it always jumps to the other block. If it is
  //      unconditional branch then containing block is dead and we will replace the instruction
  //      with a return.
  //   9. Remove Phi incoming values in the loop header that are corresponding to the back edge in
  //      the first unrolled iteration.

  verify(unroll_count > 0, "Cannot unroll loop zero times");

  const auto context = function->get_context();

  // (Step 1) Create a new exit block for the loop and make it branch to the `exit_to`.
  const auto new_loop_exit = function->create_block();
  new_loop_exit->push_instruction_back(new Branch(context, exit_to));

  // (Step 1) Change edge (`exit_from` -> `exit_to`) into (`exit_from` -> `new_loop_exit`) and fixup
  // Phis.
  replace_branch(exit_from->get_last_instruction(), exit_to, new_loop_exit);
  exit_to->replace_incoming_blocks_in_phis(exit_from, new_loop_exit);

  std::unordered_map<Instruction*, Phi*> values_escaping_loop;

  // (Step 2) Find all values that escape the loop by going through every instruction inside and
  // checking users.
  for (Block* block : loop->get_blocks()) {
    for (Instruction& instruction : *block) {
      if (instruction.is_void()) {
        continue;
      }

      // Go through every value user to find if it's used outside the loop.
      const bool is_used_outside_loop =
        any_of(instruction.users<Instruction>(),
               [&](Instruction& user) { return !loop->contains_block(user.get_block()); });

      if (is_used_outside_loop) {
        // Before unrolling all instructions outside the loop used the last iteration value.
        // Now we will have several exiting blocks so this won't work. We need to create Phi
        // in the newly created exit block for these values.
        const auto phi = new Phi(context, instruction.get_type());
        new_loop_exit->push_instruction_front(phi);

        // We already have one exiting block, so we add it to the Phi incoming list.
        phi->add_incoming(exit_from, &instruction);

        values_escaping_loop.insert({&instruction, phi});
      }
    }
  }

  // (Step 3) Replace all outside uses of values that escape the loop with newly created Phis.
  // Technically `new_loop_exit` isn't part of the loop, but we can't replace there aswell.
  for (const auto& [value, phi] : values_escaping_loop) {
    value->replace_uses_with_predicated(phi, [&](User* user) -> bool {
      const auto instruction = cast<Instruction>(user);
      return instruction && !loop->contains_block(instruction->get_block()) &&
             instruction->get_block() != new_loop_exit;
    });
  }

  std::vector<UnrolledIteration> unrolls;
  unrolls.reserve(unroll_count - 1);

  size_t mappings_count = 0;

  // (Step 4) Copy all loop blocks `unroll_count` - 1 times. The first iteration is already
  // there, so we skip it.
  for (size_t unroll = 0; unroll < unroll_count - 1; ++unroll) {
    UnrolledIteration unroll_data;

    if (mappings_count > 0) {
      unroll_data.reserve_mappings(mappings_count);
    }

    mappings_count = 0;

    // Copy blocks and instructions for this unroll instance and setup mappings.
    for (Block* original_block : loop->get_blocks()) {
      const auto new_block = function->create_block();

      unroll_data.add_mapping(original_block, new_block);
      unroll_data.add_block(new_block);
      mappings_count++;

      for (Instruction& original_instruction : *original_block) {
        Instruction* new_instruction = original_instruction.clone();

        unroll_data.add_mapping(&original_instruction, new_instruction);
        mappings_count++;

        new_block->push_instruction_back(new_instruction);
      }
    }

    // Get exiting block for this unroll instance.
    const auto new_exit_from = unroll_data.map(exit_from);

    for (Block* block : unroll_data.get_blocks()) {
      // Fixup all copied instructions.
      for (Instruction& instruction : advance_early(*block)) {
        Phi* exit_phi = nullptr;

        // (Step 5) If this value escapes the loop then get corresponding Phi instruction in
        // `new_loop_exit` for it.
        {
          if (const auto original = unroll_data.reverse_map(&instruction)) {
            const auto it = values_escaping_loop.find(original);
            if (it != values_escaping_loop.end()) {
              exit_phi = it->second;
            }
          }
        }

        if (const auto phi = cast<Phi>(instruction)) {
          const auto back_edge_value = phi->get_incoming_by_block(back_edge_from);

          // Check if this is a Phi that gets previous iteration value.
          if (back_edge_value) {
            // (Step 6) Get actual value from previously unrolled iteration.
            Value* new_value = unroll == 0 ? back_edge_value : unrolls.back().map(back_edge_value);

            // (Step 6) Replace this Phi with the value from previously unrolled iteration.
            phi->replace_uses_with_and_destroy(new_value);
            unroll_data.replace_value(phi, new_value);

            // (Step 5) Add loop escaping value incoming arm for this unroll instance.
            if (exit_phi) {
              exit_phi->add_incoming(new_exit_from, new_value);
            }

            continue;
          }
        }

        // (Step 5) Add loop escaping value incoming arm for this unroll instance.
        if (exit_phi) {
          exit_phi->add_incoming(new_exit_from, &instruction);
        }

        // Make all instruction operands point to the new values that are local to this
        // unroll instance.
        instruction.transform_operands([&](Value* operand) { return unroll_data.map(operand); });
      }
    }

    unrolls.push_back(std::move(unroll_data));
  }

  // (Step 7) Replace back edges from curent interation head to the next unrolled iteration head.
  if (unroll_count > 1) {
    const auto old_target = loop->get_header();
    const auto new_target = unrolls.front().map(loop->get_header());

    replace_branch(back_edge_from->get_last_instruction(), old_target, new_target);
  }
  if (unroll_count > 2) {
    for (size_t unroll = 0; unroll < unroll_count - 2; ++unroll) {
      // Get back edge instruction for this unroll instance.
      const auto back_edge_instruction =
        unrolls[unroll].map(back_edge_from->get_last_instruction());

      // Change edge from (this instance exiting block -> this instance header) to
      // (this instance exiting block -> next instance header).
      const auto old_target = unrolls[unroll].map(loop->get_header());
      const auto new_target = unrolls[unroll + 1].map(loop->get_header());

      replace_branch(back_edge_instruction, old_target, new_target);
    }
  }

  // (Step 8) Back edge in the last unrolled iteration is unreachable. We will remove it.
  {
    Instruction* back_edge_instruction = back_edge_from->get_last_instruction();
    Block* loop_header = loop->get_header();

    // Get actual back edge instruction and loop header for the last unrolled iteration.
    if (!unrolls.empty()) {
      UnrolledIteration& unroll = unrolls.back();

      back_edge_instruction = unroll.map(back_edge_instruction);
      loop_header = unroll.map(loop_header);
    }

    // If back edge is formed by a conditional branch then we will try to find a branch target that
    // is reachable.
    Block* new_target = nullptr;
    if (const auto cond_branch = cast<CondBranch>(back_edge_instruction)) {
      if (cond_branch->get_true_target() != loop_header) {
        new_target = cond_branch->get_true_target();
      } else if (cond_branch->get_false_target() != loop_header) {
        new_target = cond_branch->get_false_target();
      }
    }

    Instruction* new_instruction = nullptr;
    if (new_target) {
      // We have found reachable branch target so we are going to change the instruction into
      // an unconditional branch.
      new_instruction = new Branch(context, new_target);
    } else {
      // Containing block is dead and we will replace instruction with a return.
      new_instruction = new Ret(context, function->get_return_type()->is_void()
                                           ? nullptr
                                           : function->get_return_type()->undef());
    }

    back_edge_instruction->replace_with_instruction_and_destroy(new_instruction);
  }

  // (Step 9) Loop header has still some incoming values corresponding to the loop back edge. Remove
  // them.
  loop->get_header()->remove_incoming_block_from_phis(back_edge_from, false);

  // Simplify Phis after calling `remove_incoming_block_from_phis`.
  for (Phi& phi : advance_early(loop->get_header()->instructions<Phi>())) {
    utils::simplify_phi(&phi, false);
  }
}

static bool unroll_loop(Function* function,
                        const analysis::Loop* loop,
                        const DominatorTree& dominator_tree) {
  // This function will try to unroll the loop using following process:
  //   1. Check if the loop fulfils all conditions required for unrolling (ex. single back edge,
  //      single exiting edge).
  //   2. Find exit condition in the loop.
  //   3. Collect all instructions that take part in determining if loop exits or not by recursing
  //      on instruction operands
  //   4. Order collected instructions using DFS traversal on the loop.
  //   5. Interpret instructions to determine static loop count.
  //   6. Actually unroll the loop (more detailed description of this in `perform_unrolling`).

  const auto [exit_from, exit_to] = loop->get_single_exiting_edge();
  const auto back_edge_from = loop->get_single_back_edge();

  // (Step 1) We can only unroll loops with one exiting edge and one back edge.
  if (!exit_from || !exit_to || !back_edge_from) {
    return false;
  }

  // (Step 2) Get comparison instruction which determines if loop continues or exists.
  const auto exit_instruction = cast<CondBranch>(exit_from->get_last_instruction());
  if (!exit_instruction) {
    return false;
  }
  const auto exit_condition = cast<IntCompare>(exit_instruction->get_cond());
  if (!exit_condition || !loop->contains_block_skipping_sub_loops(exit_condition->get_block())) {
    return false;
  }

  // Calculate comparison instruction result that is required to continue the loop.
  bool condition_to_continue = false;
  if (exit_instruction->get_true_target() == exit_to &&
      loop->contains_block_skipping_sub_loops(exit_instruction->get_false_target())) {
    condition_to_continue = false;
  } else if (exit_instruction->get_false_target() == exit_to &&
             loop->contains_block_skipping_sub_loops(exit_instruction->get_true_target())) {
    condition_to_continue = true;
  } else {
    return false;
  }

  std::unordered_set<Instruction*> instruction_set;
  std::unordered_map<Phi*, LoopPhi> loop_phis;

  instruction_set.insert(exit_condition);

  // (Step 3) Collect all instructions that take part in loop continue or exit decision. All
  // operands of exit condition instruction should be processed (and operands of operands and so
  // on...).
  for (Value& operand : exit_condition->operands()) {
    if (const auto instruction = cast<Instruction>(operand)) {
      if (!get_loop_count_related_instructions(instruction, instruction_set, loop_phis, loop,
                                               back_edge_from, dominator_tree)) {
        return false;
      }
    }
  }

  // (Step 4) Give loop count related instructions proper order (the same as in the function), so
  // they can be processed by the `get_unroll_count`.
  const auto instructions = order_loop_count_related_instructions(loop, instruction_set);

  // (Step 5) Simulate execution of loop count related instructions to calculate amount of times
  // the loop executes.
  const auto unroll_count = get_unroll_count(instructions, loop_phis, condition_to_continue);

  // (Step 6) We can actually unroll the loop if we have successfuly calculated the iteration count.
  if (unroll_count) {
    perform_unrolling(function, loop, exit_from, exit_to, back_edge_from, *unroll_count);
    return true;
  }

  return false;
}

static bool unroll_loop_or_sub_loops(Function* function,
                                     const analysis::Loop* loop,
                                     const DominatorTree& dominator_tree) {
  // Try unrolling this loop.
  if (unroll_loop(function, loop, dominator_tree)) {
    return true;
  }

  // If it didn't work then try unrolling one of the sub-loops.
  for (const auto& sub_loop : loop->get_sub_loops()) {
    if (unroll_loop_or_sub_loops(function, sub_loop.get(), dominator_tree)) {
      return true;
    }
  }

  return false;
}

bool opt::LoopUnrolling::run(Function* function) {
  DominatorTree dominator_tree(function);

  const auto loops = analysis::analyze_function_loops(function, dominator_tree);

  bool did_something = false;

  for (const auto& loop : loops) {
    did_something |= unroll_loop_or_sub_loops(function, loop.get(), dominator_tree);
  }

  return did_something;
}