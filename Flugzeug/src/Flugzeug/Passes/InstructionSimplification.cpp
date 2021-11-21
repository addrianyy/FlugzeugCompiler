#include "InstructionSimplification.hpp"
#include "Utils/OptimizationResult.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/Passes/Utils/SimplifyPhi.hpp>

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

static OptimizationResult simplify_cmp_select_cmp_sequence(IntCompare* cmp) {
  // optimize this:
  //  v2 = cmp slt i1 v0, v1
  //  v3 = select i1 v2, i64 23, 88
  //  v4 = cmp ne i1 v3, 88
  //  ret i1 v4
  //
  // into:
  //  v2 = cmp slt i1 v0, v1
  //  ret i1 v2

  // In sequence of `cmp`, `select`, `cmp` we are matching on last compare.

  const IntPredicate pred = cmp->get_pred();
  if (pred != IntPredicate::Equal && pred != IntPredicate::NotEqual) {
    return OptimizationResult::unchanged();
  }

  // Order doesn't matter because we care only about EQ na NE predicates.
  auto select = cast<Select>(cmp->get_lhs());
  auto compared_to = cast<Constant>(cmp->get_rhs());
  if (!select || !compared_to) {
    select = cast<Select>(cmp->get_rhs());
    compared_to = cast<Constant>(cmp->get_lhs());
    if (!select || !compared_to) {
      return OptimizationResult::unchanged();
    }
  }

  const auto select_true = cast<Constant>(select->get_true_val());
  const auto select_false = cast<Constant>(select->get_false_val());
  if (!select_true || !select_false || select_true == select_false) {
    return OptimizationResult::unchanged();
  }

  bool inverted = false;

  // Get the corelation betwen first `cmp` and second `cmp`.
  // There can be 3 cases:
  // 1. second `cmp` result ==  first `cmp` result. (inverted = false)
  // 2. second `cmp` result == !first `cmp` result. (inverted = true)
  // 3. `cmps` are not corelated (in this case we exit).
  if (compared_to == select_true) {
    inverted = false;
  } else if (compared_to == select_false) {
    inverted = true;
  } else {
    return OptimizationResult::unchanged();
  }

  if (pred == IntPredicate::NotEqual) {
    inverted = !inverted;
  }

  // We know that both `cmps` are corelated with each other.
  if (!inverted) {
    cmp->replace_uses_and_destroy(select->get_cond());

    select->destroy_if_unused();

    return OptimizationResult::changed();
  } else if (const auto upper = cast<IntCompare>(select->get_cond())) {
    const auto new_cmp =
      new IntCompare(upper->get_context(), upper->get_lhs(),
                     IntCompare::inverted_predicate(upper->get_pred()), upper->get_rhs());
    cmp->replace_instruction_and_destroy(new_cmp);

    select->destroy_if_unused();
    upper->destroy_if_unused();

    return OptimizationResult::changed();
  }

  return OptimizationResult::unchanged();
}

class Simplifier : public InstructionVisitor {
public:
  OptimizationResult visit_unary_instr(Argument<UnaryInstr> unary) {
    return OptimizationResult::unchanged();
  }
  OptimizationResult visit_binary_instr(Argument<BinaryInstr> binary) {
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

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_int_compare(Argument<IntCompare> int_compare) {
    if (const auto result = simplify_cmp_select_cmp_sequence(int_compare)) {
      return result;
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_cast(Argument<Cast> cast) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_cond_branch(Argument<CondBranch> cond_branch) {
    return OptimizationResult::unchanged();
  }
  OptimizationResult visit_select(Argument<Select> select) {
    return OptimizationResult::unchanged();
  }
  OptimizationResult visit_stackalloc(Argument<StackAlloc> stackalloc) {
    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_phi(Argument<Phi> phi) {
    if (utils::simplify_phi(phi, true)) {
      return OptimizationResult::changed();
    }

    return OptimizationResult::unchanged();
  }

  OptimizationResult visit_load(Argument<Load> load) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_store(Argument<Store> store) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_call(Argument<Call> call) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_branch(Argument<Branch> branch) {
    return OptimizationResult::unchanged();
  }
  OptimizationResult visit_ret(Argument<Ret> ret) { return OptimizationResult::unchanged(); }
  OptimizationResult visit_offset(Argument<Offset> offset) {
    return OptimizationResult::unchanged();
  }
};

bool InstructionSimplification::run(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    for (Instruction& instruction : dont_invalidate_current(block)) {
      Simplifier simplifier;

      Instruction* current_instruction = &instruction;
      while (current_instruction) {
        if (const auto result = visitor::visit_instruction(current_instruction, simplifier)) {
          current_instruction = nullptr;

          if (const auto replacement = result.get_replacement()) {
            if (const auto new_instruction = cast<Instruction>(replacement)) {
              if (!new_instruction->get_block()) {
                current_instruction = new_instruction;
              }
            }

            instruction.replace_instruction_or_uses_and_destroy(replacement);
          }

          did_something = true;
        } else {
          break;
        }
      }
    }
  }

  return did_something;
}