#include "ConditionalCommonOperationExtraction.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

class CommonOperation {
  enum class Kind {
    Unknown,
    Unary,
    Binary,
  };

  Kind kind = Kind::Unknown;

  BinaryOp binary_op = BinaryOp::Add;
  Value* binary_rhs = nullptr;

  UnaryOp unary_op = UnaryOp::Neg;

public:
  bool add_case(Value* value) {
    switch (kind) {
    case Kind::Unknown: {
      if (const auto unary = cast<UnaryInstr>(value)) {
        kind = Kind::Unary;
        unary_op = unary->get_op();

        return true;
      }

      if (const auto binary = cast<BinaryInstr>(value)) {
        kind = Kind::Binary;
        binary_op = binary->get_op();
        binary_rhs = binary->get_rhs();

        return true;
      }

      return false;
    }

    case Kind::Unary: {
      const auto unary = cast<UnaryInstr>(value);
      return unary && unary->get_op() == unary_op;
    }

    case Kind::Binary: {
      const auto binary = cast<BinaryInstr>(value);
      return binary && binary->get_op() == binary_op && binary->get_rhs() == binary_rhs;
    }

    default:
      unreachable();
    }
  }

  Value* extract_argument(Value* value) {
    if (const auto unary = cast<UnaryInstr>(value)) {
      return unary->get_val();
    }

    if (const auto binary = cast<BinaryInstr>(value)) {
      return binary->get_lhs();
    }

    unreachable();
  }

  Instruction* create_instruction(Value* argument) {
    switch (kind) {
    case Kind::Unary: {
      return new UnaryInstr(argument->get_context(), unary_op, argument);
    }

    case Kind::Binary: {
      return new BinaryInstr(argument->get_context(), argument, binary_op, binary_rhs);
    }

    default:
      unreachable();
    }
  }
};

static Instruction* handle_phi(Phi* phi) {
  CommonOperation operation;

  const bool optimizable =
    all_of(*phi, [&](Phi::Incoming incoming) { return operation.add_case(incoming.value); });

  if (!optimizable) {
    return nullptr;
  }

  for (const auto incoming : *phi) {
    const auto instruction = cast<Instruction>(incoming.value);
    phi->replace_incoming_for_block(incoming.block, operation.extract_argument(instruction));
    instruction->destroy_if_unused();
  }

  return operation.create_instruction(phi);
}

static Instruction* handle_select(Select* select) {
  CommonOperation operation;

  if (!operation.add_case(select->get_true_val()) || !operation.add_case(select->get_false_val())) {
    return nullptr;
  }

  {
    const auto instruction = cast<Instruction>(select->get_true_val());
    select->set_true_val(operation.extract_argument(instruction));
    instruction->destroy_if_unused();
  }

  {
    const auto instruction = cast<Instruction>(select->get_false_val());
    select->set_false_val(operation.extract_argument(instruction));
    instruction->destroy_if_unused();
  }

  return operation.create_instruction(select);
}

bool opt::ConditionalCommonOperationExtraction::run(Function* function) {
  //   c ? (x + 10) : (y + 10)
  // =>
  //   (c ? x : y) + 10

  bool did_something = false;

  for (Instruction& instruction : advance_early(function->instructions())) {
    Instruction* final = nullptr;

    if (const auto phi = cast<Phi>(instruction)) {
      final = handle_phi(phi);
    } else if (const auto select = cast<Select>(instruction)) {
      final = handle_select(select);
    }

    if (!final) {
      continue;
    }

    final->insert_after(&instruction);

    instruction.replace_uses_with_predicated(final, [&](User* user) { return user != final; });

    did_something = true;
  }

  return did_something;
}