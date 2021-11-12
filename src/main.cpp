#include <Core/Log.hpp>
#include <Core/NonInvalidatingIterator.hpp>
#include <IR/ConsolePrinter.hpp>
#include <IR/Context.hpp>
#include <IR/Function.hpp>
#include <IR/InstructionInserter.hpp>
#include <IR/User.hpp>
#include <Passes/DeadCodeElimination.hpp>
#include <Passes/MemoryToSSA.hpp>
#include <iostream>
#include <unordered_set>

#include "FunctionsCreator.hpp"

using namespace flugzeug;

static bool is_pow2(uint64_t x) { return (x != 0) && !(x & (x - 1)); }

static uint64_t bin_log2(uint64_t x) {
  for (uint64_t i = 1; i < 64; ++i) {
    if ((x >> i) & 1) {
      return i;
    }
  }

  return 0;
}

static Value* optimize_binary_instruction(BinaryInstr* binary) {
  const auto context = binary->get_context();
  const auto zero = context->get_constant(binary->get_type(), 0);

  if (binary->is(BinaryOp::Sub) && binary->get_lhs() == binary->get_rhs()) {
    // sub X, X == 0
    return zero;
  } else if (binary->is(BinaryOp::Add) && binary->get_rhs()->is_zero()) {
    // add X, 0 == X
    return binary->get_lhs();
  } else if (binary->is(BinaryOp::Mul)) {
    if (const auto multiplier_v = cast<Constant>(binary->get_rhs())) {
      const auto multiplier = multiplier_v->get_constant_u();
      if (multiplier == 0) {
        // mul X, 0 == 0
        return zero;
      } else if (multiplier == 1) {
        // mul X, 1 == X
        return binary->get_lhs();
      } else if (is_pow2(multiplier)) {
        // mul X, Y (if Y is power of 2) == shl X, log2(Y)
        const auto shift_amount = context->get_constant(binary->get_type(), bin_log2(multiplier));
        return new BinaryInstr(context, binary->get_lhs(), BinaryOp::Shl, shift_amount);
      }
    }
  }

  return nullptr;
}

static void test_optimization(Function* function) {
  for (Block& block : *function) {
    for (Instruction& instruction : dont_invalidate_current(block)) {
      if (auto binary = cast<BinaryInstr>(&instruction)) {
        if (auto replacement = optimize_binary_instruction(binary)) {
          const auto i = cast<Instruction>(replacement);

          if (i && !i->get_block()) {
            binary->replace_instruction(i);
          } else {
            binary->replace_uses_and_destroy(replacement);
          }
        }
      }
    }
  }
}

int old_main() {
  Context context;

  Type* i64 = context.get_i64_ty();

  auto f = context.create_function(i64, "test", std::vector{i64, i64});

  auto param_a = f->get_parameter(0);
  auto param_b = f->get_parameter(1);

  auto entry = f->create_block();
  auto if_then = f->create_block();
  auto if_else = f->create_block();
  auto merge = f->create_block();

  InstructionInserter inserter(entry);

  auto ptr = inserter.stack_alloc(i64);
  inserter.store(ptr, context.get_constant(i64, 1));
  auto cond = inserter.compare_eq(param_a, param_b);
  inserter.cond_branch(cond, if_then, if_else);

  inserter.set_insertion_block(if_then);
  inserter.store(ptr, context.get_constant(i64, 2));
  inserter.branch(merge);

  inserter.set_insertion_block(if_else);
  inserter.store(ptr, context.get_constant(i64, 3));
  inserter.branch(merge);

  inserter.set_insertion_block(merge);
  auto loaded = inserter.load(ptr);
  inserter.add(param_a, param_b);
  inserter.ret(loaded);

  //  MemoryToSSA::run(f);
  DeadCodeElimination::run(f);

  ConsolePrinter printer(ConsolePrinter::Variant::Colorful);
  f->print(printer);

  f->destroy();

  return 0;
}

int main() {
  Context context;

  const auto functions = create_functions(&context);

  ConsolePrinter printer(ConsolePrinter::Variant::Colorful);

  for (Function* f : functions) {
    if (f->is_extern()) {
      continue;
    }

    f->print(printer);
    printer.newline();
  }

  for (Function* f : functions) {
    f->destroy();
  }
}
