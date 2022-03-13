#include "User.hpp"
#include "Block.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace flugzeug;

User::~User() {
  for (size_t i = 0; i < get_operand_count(); ++i) {
    set_operand(i, nullptr);
  }

  for (auto use : uses_for_operands) {
    verify(!use->next && !use->previous, "Use is still inserted at destructor");

    if (use->heap_allocated) {
      delete use;
    }
  }
}

void User::remove_phi_incoming_helper(size_t incoming_index) {
  const auto incoming_count = get_operand_count() / 2;
  const auto start_operand = incoming_index * 2;

  // Zero out 2 operands.
  set_operand(start_operand + 0, nullptr);
  set_operand(start_operand + 1, nullptr);

  if (incoming_index + 1 != incoming_count) {
    // It's not the last index, we need to move operands.
    // We need to be careful to not invalidate Users iterators.

    auto saved_u1 = uses_for_operands[start_operand + 0];
    auto saved_u2 = uses_for_operands[start_operand + 1];

    const auto offset = std::ptrdiff_t(start_operand);
    std::copy(used_operands.begin() + offset + 2, used_operands.end(),
              used_operands.begin() + offset);
    std::copy(uses_for_operands.begin() + offset + 2,
              uses_for_operands.begin() + std::ptrdiff_t(get_operand_count()),
              uses_for_operands.begin() + offset);

    used_operands[get_operand_count() - 2] = nullptr;
    used_operands[get_operand_count() - 1] = nullptr;

    uses_for_operands[get_operand_count() - 2] = saved_u1;
    uses_for_operands[get_operand_count() - 1] = saved_u2;

    for (size_t i = start_operand; i < get_operand_count(); ++i) {
      uses_for_operands[i]->operand_index = uint32_t(i);
    }
  }

  set_operand_count(get_operand_count() - 2);
}

void User::adjust_uses_count(size_t count) {
  if (count > uses_for_operands.size()) {
    const auto previous_size = uses_for_operands.size();

    uses_for_operands.resize(count);

    for (size_t i = previous_size; i < count; ++i) {
      if (i < static_use_count) {
        auto use = &static_uses[i];

        use->user = this;
        use->operand_index = uint32_t(i);
        use->heap_allocated = false;

        uses_for_operands[i] = use;
      } else {
        uses_for_operands[i] = new detail::Use(this, i);
        uses_for_operands[i]->heap_allocated = true;
      }
    }
  }
}

void User::reserve_operands(size_t count) {
  if (count > used_operands.capacity()) {
    used_operands.reserve(count);
  }

  adjust_uses_count(count);
}

void User::set_operand_count(size_t count) {
  const auto before_count = get_operand_count();
  if (before_count == count) {
    return;
  }

  if (before_count > count) {
    for (size_t i = count; i < before_count; ++i) {
      verify(used_operands[i] == nullptr, "Tried to remove exsiting operand.");
    }
  }

  used_operands.resize(count, nullptr);

  adjust_uses_count(count);
}

void User::grow_operand_count(size_t grow) { set_operand_count(get_operand_count() + grow); }

size_t User::get_operand_count() const { return used_operands.size(); }

Value* User::get_operand(size_t index) {
  verify(index < get_operand_count(), "Tried to use out of bounds operand.");
  return used_operands[index];
}

const Value* User::get_operand(size_t index) const {
  verify(index < get_operand_count(), "Tried to use out of bounds operand.");
  return used_operands[index];
}

void User::set_operand(size_t index, Value* operand) {
  verify(index < get_operand_count(), "Tried to use out of bounds operand.");

  Value* old_operand = used_operands[index];
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

  used_operands[index] = operand;
}

bool User::uses_value(Value* value) const {
  for (size_t i = 0; i < get_operand_count(); ++i) {
    if (used_operands[i] == value) {
      return true;
    }
  }

  return false;
}

bool User::replace_operands(Value* old_value, Value* new_value) {
  if (old_value == new_value) {
    return true;
  }

  verify(old_value->is_same_type_as(new_value),
         "Cannot replace operands with value of different type");

  const auto block = cast<Block>(new_value);

  size_t replace_count = 0;

  for (size_t i = 0; i < get_operand_count(); ++i) {
    if (used_operands[i] == old_value) {
      set_operand(i, new_value);
      replace_count++;
    }
  }

  if (block && replace_count > 0) {
    deduplicate_phi_incoming_blocks(block, this);
  }

  return replace_count > 0;
}
