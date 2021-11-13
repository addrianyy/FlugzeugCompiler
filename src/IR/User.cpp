#include "User.hpp"

#include <Core/Error.hpp>

using namespace flugzeug;

User::~User() {
  for (size_t i = 0; i < get_operand_count(); ++i) {
    set_operand(i, nullptr);
  }
}

size_t User::get_operand_count() const { return operands.size(); }

Value* User::get_operand(size_t index) {
  verify(index < get_operand_count(), "Tried to use out of bounds operand.");
  return operands[index];
}

const Value* User::get_operand(size_t index) const {
  verify(index < get_operand_count(), "Tried to use out of bounds operand.");
  return operands[index];
}

void User::set_operand_count(size_t count) {
  const auto current_count = get_operand_count();
  if (current_count == count) {
    return;
  }

  if (current_count < count) {
    operands.resize(count, nullptr);
  }

  for (unsigned i = count; i < current_count; ++i) {
    verify(get_operand(i) == nullptr, "Tried to remove exsiting operand.");
  }

  operands.resize(count);
}

void User::grow_operand_count(size_t grow) { set_operand_count(get_operand_count() + grow); }

void User::set_operand(size_t index, Value* operand) {
  verify(index < get_operand_count(), "Tried to use out of bounds operand.");

  Value* old_operand = operands[index];
  if (old_operand == operand) {
    return;
  }

  const auto use = Use{this, index};

  if (old_operand) {
    old_operand->remove_use(use);
  }

  if (operand) {
    operand->add_use(use);
  }

  operands[index] = operand;
}