#include "Instructions.hpp"
#include "Function.hpp"

using namespace flugzeug;

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

bool Phi::remove_incoming_opt(const Block* block) {
  size_t index;
  if (!index_for_block(block, index)) {
    return false;
  }

  const size_t last_index = get_incoming_count() - 1;

  if (index != last_index) {
    set_operand(get_block_index(index), get_operand(get_block_index(last_index)));
    set_operand(get_value_index(index), get_operand(get_value_index(last_index)));
  }

  // Remove last entry.
  set_operand(get_block_index(last_index), nullptr);
  set_operand(get_value_index(last_index), nullptr);
  set_operand_count(get_operand_count() - 2);

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
