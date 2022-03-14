#include "BrainfuckLoopOptimization.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/IR/Patterns.hpp>

#include <Flugzeug/Passes/Analysis/Loops.hpp>
#include <Flugzeug/Passes/Utils/SimplifyPhi.hpp>

using namespace flugzeug;

struct BrainfuckAdd {
  BinaryInstr* instruction;
  Value* normal_operand;
  Value* foreign_operand;
};

struct BrainfuckLoop {
  IntCompare* compare = nullptr;
  CondBranch* branch = nullptr;
  Phi* iteration_count = nullptr;

  Block* exit_block = nullptr;

  std::vector<Phi*> phis;
  std::vector<BrainfuckAdd> adds;
};

static bool get_brainfuck_loop(Block* block, BrainfuckLoop& loop) {
  loop.branch = cast<CondBranch>(block->get_last_instruction());
  if (!loop.branch || loop.branch->get_true_target() != block) {
    return false;
  }

  loop.exit_block = loop.branch->get_false_target();

  if (!match_pattern(loop.branch->get_cond(),
                     pat::compare_ne(loop.compare, pat::value(loop.iteration_count), pat::one()))) {
    return false;
  }

  if (loop.iteration_count->get_block() != block) {
    return false;
  }

  const auto step = loop.iteration_count->get_incoming_by_block(block);
  if (!match_pattern(step, pat::add(pat::exact(loop.iteration_count), pat::negative_one()))) {
    return false;
  }

  const auto is_foreign = [&](Value* value) {
    if (const auto instruction = cast<Instruction>(value)) {
      return instruction->get_block() != block;
    }

    return true;
  };

  for (Instruction& instruction : *block) {
    if (const auto phi = cast<Phi>(instruction)) {
      loop.phis.push_back(phi);
      continue;
    }

    if (&instruction == loop.compare || &instruction == loop.branch) {
      continue;
    }

    Value* normal;
    Value* foreign;
    if (!match_pattern(instruction, pat::add(pat::value(normal), pat::value(foreign)))) {
      return false;
    }

    if (!is_foreign(foreign)) {
      std::swap(normal, foreign);
      if (!is_foreign(foreign)) {
        return false;
      }
    }

    loop.adds.push_back(BrainfuckAdd{
      .instruction = cast<BinaryInstr>(instruction),
      .normal_operand = normal,
      .foreign_operand = foreign,
    });
  }

  return true;
}

static bool optimize_loop(const analysis::Loop* loop) {
  const auto block = loop->get_header();
  if (loop->get_blocks().size() != 1) {
    return false;
  }

  BrainfuckLoop bf_loop{};
  if (!get_brainfuck_loop(block, bf_loop)) {
    return false;
  }

  bf_loop.branch->replace_with_instruction_and_destroy(
    new Branch(block->get_context(), bf_loop.exit_block));
  bf_loop.compare->destroy_if_unused();

  for (const auto& add : bf_loop.adds) {
    const auto added_value = new BinaryInstr(block->get_context(), add.foreign_operand,
                                             BinaryOp::Mul, bf_loop.iteration_count);
    added_value->insert_before(add.instruction);
    add.instruction->set_new_operands(add.normal_operand, BinaryOp::Add, added_value);
  }

  for (const auto phi : bf_loop.phis) {
    phi->remove_incoming(block);
    utils::simplify_phi(phi, true);
  }

  return true;
}

static bool optimize_loop_or_sub_loops(const analysis::Loop* loop) {
  if (optimize_loop(loop)) {
    return true;
  }

  bool optimized_sub_loop = false;

  for (const auto& sub_loop : loop->get_sub_loops()) {
    optimized_sub_loop |= optimize_loop_or_sub_loops(sub_loop.get());
  }

  return optimized_sub_loop;
}

bool bf::BrainfuckLoopOptimization::run(Function* function) {
  const auto loops = analysis::analyze_function_loops(function);

  bool did_something = false;

  for (const auto& loop : loops) {
    did_something |= optimize_loop_or_sub_loops(loop.get());
  }

  return did_something;
}