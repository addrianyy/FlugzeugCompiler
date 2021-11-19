#include "GeneralSimplification.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>

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

class Simplifier : public InstructionVisitor {
  bool& did_something;

public:
  explicit Simplifier(bool& did_something) : did_something(did_something) {}

  Value* visit_unary_instr(Argument<UnaryInstr> unary) { return nullptr; }

  Value* visit_binary_instr(Argument<BinaryInstr> binary) {
    const auto zero = binary->get_type()->get_zero();

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
          const auto shift_amount = binary->get_type()->get_constant(bin_log2(multiplier));
          return new BinaryInstr(binary->get_context(), binary->get_lhs(), BinaryOp::Shl,
                                 shift_amount);
        }
      }
    }

    return nullptr;
  }

  Value* visit_int_compare(Argument<IntCompare> int_compare) { return nullptr; }
  Value* visit_load(Argument<Load> load) { return nullptr; }
  Value* visit_store(Argument<Store> store) { return nullptr; }
  Value* visit_call(Argument<Call> call) { return nullptr; }
  Value* visit_branch(Argument<Branch> branch) { return nullptr; }
  Value* visit_cond_branch(Argument<CondBranch> cond_branch) { return nullptr; }
  Value* visit_stackalloc(Argument<StackAlloc> stackalloc) { return nullptr; }
  Value* visit_ret(Argument<Ret> ret) { return nullptr; }
  Value* visit_offset(Argument<Offset> offset) { return nullptr; }
  Value* visit_cast(Argument<Cast> cast) { return nullptr; }
  Value* visit_select(Argument<Select> select) { return nullptr; }

  Value* visit_phi(Argument<Phi> phi) {
    did_something = utils::simplify_phi(phi, true);
    return nullptr;
  }
};

bool GeneralSimplification::run(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    for (Instruction& instruction : dont_invalidate_current(block)) {
      Simplifier simplifier(did_something);

      if (const auto simplified = visitor::visit_instruction(&instruction, simplifier)) {
        instruction.replace_instruction_or_uses_and_destroy(simplified);
        did_something = true;
      }
    }
  }

  return did_something;
}
