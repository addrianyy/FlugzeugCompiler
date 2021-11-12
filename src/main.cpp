#include <Core/NonInvalidatingIterator.hpp>
#include <IR/ConsolePrinter.hpp>
#include <IR/Context.hpp>
#include <IR/Function.hpp>
#include <IR/InstructionInserter.hpp>
#include <IR/User.hpp>
#include <iostream>

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

int main() {
  Context context;

  auto f = context.create_function(Type::Kind::I32, "test", std::vector{Type(Type::Kind::I64)});

  Constant* one = context.get_constant(Type::Kind::I64, 1);
  Constant* two = context.get_constant(Type::Kind::I64, 2);
  Constant* six = context.get_constant(Type::Kind::I64, 6);
  Constant* nine = context.get_constant(Type::Kind::I32, 9);
  Constant* eight = context.get_constant(Type::Kind::I64, 8);
  Value* undef = context.get_undef(Type::Kind::I32);

  InstructionInserter inserter(f->create_block());

  auto true_block = f->create_block();
  auto false_block = f->create_block();
  auto merge = f->create_block();

  auto ab = f->create_block();
  auto bb = f->create_block();
  auto cb = f->create_block();

  auto calculated = inserter.mul(inserter.add(f->get_parameter(0), one), eight);
  auto x = inserter.sub(calculated, calculated);
  calculated = inserter.add(calculated, x);
  auto cond = inserter.compare_sgt(calculated, six);
  inserter.cond_branch(cond, true_block, false_block);

  inserter.set_insertion_block(true_block);
  inserter.branch(merge);

  inserter.set_insertion_block(false_block);
  inserter.branch(merge);

  inserter.set_insertion_block(merge);
  auto v = inserter.phi({Phi::Incoming{true_block, nine}, Phi::Incoming{false_block, six}});
  auto y = inserter.phi({Phi::Incoming{ab, nine}, Phi::Incoming{bb, six}, Phi::Incoming{cb, two}});
  inserter.ret(v);

  test_optimization(f);

  ab->destroy();

  // calculated->replace_uses(context.get_constant(calculated->get_type(), 123));

  ConsolePrinter printer(ConsolePrinter::Variant::Colorful);
  f->print(printer);

  f->destroy();
}