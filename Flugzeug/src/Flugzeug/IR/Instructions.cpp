#include "Instructions.hpp"
#include "Function.hpp"

using namespace flugzeug;

bool BinaryInstr::is_binary_op_commutative(BinaryOp op) {
  switch (op) {
    case BinaryOp::Add:
    case BinaryOp::Mul:
    case BinaryOp::And:
    case BinaryOp::Or:
    case BinaryOp::Xor:
      return true;

    default:
      return false;
  }
}

IntPredicate IntCompare::inverted_predicate(IntPredicate pred) {
  switch (pred) {
    case IntPredicate::Equal:
      return IntPredicate::NotEqual;
    case IntPredicate::NotEqual:
      return IntPredicate::Equal;
    case IntPredicate::GtS:
      return IntPredicate::LteS;
    case IntPredicate::GteS:
      return IntPredicate::LtS;
    case IntPredicate::GtU:
      return IntPredicate::LteU;
    case IntPredicate::GteU:
      return IntPredicate::LtU;
    case IntPredicate::LtS:
      return IntPredicate::GteS;
    case IntPredicate::LteS:
      return IntPredicate::GtS;
    case IntPredicate::LtU:
      return IntPredicate::GteU;
    case IntPredicate::LteU:
      return IntPredicate::GtU;
    default:
      unreachable();
  }
}

IntPredicate IntCompare::swapped_order_predicate(IntPredicate pred) {
  switch (pred) {
    case IntPredicate::Equal:
      return IntPredicate::Equal;
    case IntPredicate::NotEqual:
      return IntPredicate::NotEqual;
    case IntPredicate::GtS:
      return IntPredicate::LtS;
    case IntPredicate::GteS:
      return IntPredicate::LteS;
    case IntPredicate::GtU:
      return IntPredicate::LtU;
    case IntPredicate::GteU:
      return IntPredicate::LteU;
    case IntPredicate::LtS:
      return IntPredicate::GtS;
    case IntPredicate::LteS:
      return IntPredicate::GteS;
    case IntPredicate::LtU:
      return IntPredicate::GtU;
    case IntPredicate::LteU:
      return IntPredicate::GteU;
    default:
      unreachable();
  }
}

Call::Call(Context* context, Function* function, const std::vector<Value*>& arguments)
    : Instruction(context, Value::Kind::Call, function->return_type()) {
  set_operand_count(arguments.size() + 1);

  set_operand(0, function);

  for (size_t i = 0; i < arguments.size(); ++i) {
    set_operand(i + 1, arguments[i]);
  }
}

Function* Call::callee() {
  return cast<Function>(operand(0));
}

const Function* Call::callee() const {
  return cast<Function>(operand(0));
}

bool Phi::index_for_block(const Block* block, size_t& index) const {
  index = 0;

  for (size_t i = 0; i < incoming_count(); ++i) {
    if (operand(get_block_index(i)) == block) {
      index = i;
      return true;
    }
  }

  return false;
}

void Phi::remove_incoming_by_index(size_t index) {
  // This needs special handling in User class.
  remove_phi_incoming_helper(index);
}

Value* Phi::single_incoming_value() {
  Value* single_incoming = nullptr;

  for (size_t i = 0; i < incoming_count(); ++i) {
    const auto value = get_incoming_value(i);
    if (value == this) {
      continue;
    }

    if (single_incoming && value != single_incoming) {
      return nullptr;
    }

    single_incoming = value;
  }

  return single_incoming;
}

Value* Phi::remove_incoming_opt(const Block* block) {
  size_t index;
  if (!index_for_block(block, index)) {
    return nullptr;
  }

  const auto value = get_incoming_value(index);
  remove_incoming_by_index(index);
  return value;
}

Value* Phi::remove_incoming(const Block* block) {
  const auto result = remove_incoming_opt(block);
  verify(result, "Unknown block passed to remove incoming");
  return result;
}

void Phi::add_incoming(Block* block, Value* value) {
  size_t prev_index;
  if (index_for_block(block, prev_index)) {
    verify(operand(get_value_index(prev_index)) == value,
           "Tried to add 2 same blocks to the Phi instruction.");
    return;
  }

  const auto index = incoming_count();
  grow_operand_count(2);
  set_operand(get_block_index(index), block);
  set_operand(get_value_index(index), value);
}

Value* Phi::incoming_for_block(const Block* block) {
  size_t index;
  if (!index_for_block(block, index)) {
    return nullptr;
  }
  return operand(get_value_index(index));
}

const Value* Phi::incoming_for_block(const Block* block) const {
  size_t index;
  if (!index_for_block(block, index)) {
    return nullptr;
  }
  return operand(get_value_index(index));
}

void Phi::replace_incoming_for_block(const Block* block, Value* new_incoming) {
  size_t index;
  verify(index_for_block(block, index), "No incoming found for given block");
  set_operand(get_value_index(index), new_incoming);
}

bool Phi::replace_incoming_block_opt(const Block* old_incoming, Block* new_incoming) {
  if (old_incoming == new_incoming) {
    return false;
  }

  size_t old_incoming_index;
  if (!index_for_block(old_incoming, old_incoming_index)) {
    return false;
  }

  size_t new_incoming_index;
  if (index_for_block(new_incoming, new_incoming_index)) {
    verify(
      operand(get_value_index(new_incoming_index)) == operand(get_value_index(old_incoming_index)),
      "Cannot duplicate blocks in Phi.");
    remove_incoming_by_index(old_incoming_index);
    return true;
  }

  set_operand(get_block_index(old_incoming_index), new_incoming);
  return true;
}

void Phi::replace_incoming_block(const Block* old_incoming, Block* new_incoming) {
  if (old_incoming == new_incoming) {
    return;
  }

  verify(replace_incoming_block_opt(old_incoming, new_incoming),
         "Unknown block passed to replace incoming");
}