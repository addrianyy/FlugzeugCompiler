#include "Instructions.hpp"
#include "Function.hpp"

using namespace flugzeug;

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

Call::Call(Context* context, Function* function, const std::vector<Value*>& arguments)
    : Instruction(context, Value::Kind::Call, function->get_return_type()), target(function) {
  set_operand_count(arguments.size());

  for (size_t i = 0; i < arguments.size(); ++i) {
    set_operand(i, arguments[i]);
  }
}

bool Phi::index_for_block(const Block* block, size_t& index) const {
  index = 0;

  for (size_t i = 0; i < get_incoming_count(); ++i) {
    if (get_operand(get_block_index(i)) == block) {
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

Value* Phi::get_single_incoming_value() {
  Value* single_incoming = nullptr;

  for (size_t i = 0; i < get_incoming_count(); ++i) {
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

bool Phi::remove_incoming_opt(const Block* block) {
  size_t index;
  if (!index_for_block(block, index)) {
    return false;
  }

  remove_incoming_by_index(index);
  return true;
}

void Phi::remove_incoming(const Block* block) {
  verify(remove_incoming_opt(block), "Unknown block passed to remove incoming");
}

void Phi::add_incoming(Block* block, Value* value) {
  size_t prev_index;
  if (index_for_block(block, prev_index)) {
    verify(get_operand(get_value_index(prev_index)) == value,
           "Tried to add 2 same blocks to PHI instruction.");
    return;
  }

  const auto index = get_incoming_count();
  grow_operand_count(2);
  set_operand(get_block_index(index), block);
  set_operand(get_value_index(index), value);
}

Value* Phi::get_incoming_by_block(const Block* block) {
  size_t index;
  if (!index_for_block(block, index)) {
    return nullptr;
  }
  return get_operand(get_value_index(index));
}

const Value* Phi::get_incoming_by_block(const Block* block) const {
  size_t index;
  if (!index_for_block(block, index)) {
    return nullptr;
  }
  return get_operand(get_value_index(index));
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
    verify(get_operand(get_value_index(new_incoming_index)) ==
             get_operand(get_value_index(old_incoming_index)),
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