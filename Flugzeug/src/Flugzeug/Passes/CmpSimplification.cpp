#include "CmpSimplification.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

bool CmpSimplification::simplify_cmp(IntCompare* cmp) {
  // In sequence of `cmp`, `select`, `cmp` we are matching on last compare.

  const IntPredicate pred = cmp->get_pred();
  if (pred != IntPredicate::Equal && pred != IntPredicate::NotEqual) {
    return false;
  }

  // Order doesn't matter because we care only about EQ na NE predicates.
  auto select = cast<Select>(cmp->get_lhs());
  auto compared_to = cast<Constant>(cmp->get_rhs());
  if (!select || !compared_to) {
    select = cast<Select>(cmp->get_rhs());
    compared_to = cast<Constant>(cmp->get_lhs());
    if (!select || !compared_to) {
      return false;
    }
  }

  const auto select_true = cast<Constant>(select->get_true_val());
  const auto select_false = cast<Constant>(select->get_false_val());
  if (!select_true || !select_false || select_true == select_false) {
    return false;
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
    return false;
  }

  if (pred == IntPredicate::NotEqual) {
    inverted = !inverted;
  }

  // We know that both `cmps` are corelated with each other.
  if (!inverted) {
    cmp->replace_uses_and_destroy(select->get_cond());

    select->destroy_if_unused();

    return true;
  } else if (const auto upper = cast<IntCompare>(select->get_cond())) {
    const auto new_cmp =
      new IntCompare(upper->get_context(), upper->get_lhs(),
                     IntCompare::inverted_predicate(upper->get_pred()), upper->get_rhs());
    cmp->replace_instruction_and_destroy(new_cmp);

    select->destroy_if_unused();
    upper->destroy_if_unused();

    return true;
  }

  return false;
}

bool CmpSimplification::run(Function* function) {
  bool did_something = false;

  // optimize this:
  //  v2 = cmp slt i1 v0, v1
  //  v3 = select i1 v2, i64 23, 88
  //  v4 = cmp ne i1 v3, 88
  //  ret i1 v4
  //
  // into:
  //  v2 = cmp slt i1 v0, v1
  //  ret i1 v2

  for (Block& block : *function) {
    for (Instruction& instruction : dont_invalidate_current(block)) {
      if (const auto cmp = cast<IntCompare>(instruction)) {
        did_something |= simplify_cmp(cmp);
      }
    }
  }

  return did_something;
}