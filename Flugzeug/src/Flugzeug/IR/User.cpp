#include "User.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace flugzeug;

User::~User() {
  for (size_t i = 0; i < get_operand_count(); ++i) {
    set_operand(i, nullptr);
  }

  for (Use* use : uses_for_operands) {
    verify(!use->next && !use->previous && !use->valid, "Use is still inserted at destructor");
    delete use;
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
  const auto before_count = get_operand_count();
  if (before_count == count) {
    return;
  }

  if (before_count > count) {
    for (size_t i = count; i < before_count; ++i) {
      verify(get_operand(i) == nullptr, "Tried to remove exsiting operand.");
    }
  }

  operands.resize(count, nullptr);

  if (count > before_count) {
    uses_for_operands.resize(count);

    for (size_t i = before_count; i < count; ++i) {
      uses_for_operands[i] = new Use(this, i);
    }
  }
}

void User::remove_phi_incoming_helper(size_t incoming_index) {
  const auto incoming_count = get_operand_count() / 2;
  const auto start_operand = incoming_index * 2;

  // Remove 2 operands.
  set_operand(start_operand + 0, nullptr);
  set_operand(start_operand + 1, nullptr);

  if (incoming_index + 1 != incoming_count) {
    // It's not the last index, we need to move operands.
    // We need to be careful to not invalidate Users iterators.

    Use* saved_u1 = uses_for_operands[start_operand + 0];
    Use* saved_u2 = uses_for_operands[start_operand + 1];

    verify(!saved_u1->valid && !saved_u2->valid, "Uses already inserted");

    const auto offset = std::ptrdiff_t(start_operand);
    std::copy(operands.begin() + offset + 2, operands.end(), operands.begin() + offset);
    std::copy(uses_for_operands.begin() + offset + 2, uses_for_operands.end(),
              uses_for_operands.begin() + offset);

    operands[get_operand_count() - 2] = nullptr;
    operands[get_operand_count() - 1] = nullptr;

    uses_for_operands[get_operand_count() - 2] = saved_u1;
    uses_for_operands[get_operand_count() - 1] = saved_u2;

    for (size_t i = start_operand; i < get_operand_count(); ++i) {
      uses_for_operands[i]->operand_index = i;
    }
  }

  set_operand_count(get_operand_count() - 2);
}

void User::grow_operand_count(size_t grow) { set_operand_count(get_operand_count() + grow); }

void User::set_operand(size_t index, Value* operand) {
  verify(index < get_operand_count(), "Tried to use out of bounds operand.");

  Value* old_operand = operands[index];
  if (old_operand == operand) {
    return;
  }

  const auto use = uses_for_operands[index];

  if (old_operand) {
    old_operand->remove_use(use);
  }

  if (operand) {
    operand->add_use(use);
  }

  operands[index] = operand;
}