#include "Value.hpp"
#include "Block.hpp"
#include "Function.hpp"
#include "Instructions.hpp"

using namespace flugzeug;

Block* Value::cast_to_block(Value* value) { return cast<Block>(value); }

void Value::set_user_operand(User* user, size_t operand_index, Value* value) {
  return user->set_operand(operand_index, value);
}

void Value::add_use(detail::Use* use) {
  uses.add_use(use);

  if (use->get_user() != this) {
    user_count_excluding_self++;
  }

  if (const auto block = cast<Block>(this)) {
    block->on_added_block_user(use->get_user());
  }
}

void Value::remove_use(detail::Use* use) {
  uses.remove_use(use);

  if (use->get_user() != this) {
    user_count_excluding_self--;
  }

  if (const auto block = cast<Block>(this)) {
    block->on_removed_block_user(use->get_user());
  }
}

bool Value::is_phi() const { return cast<Phi>(this); }

void Value::deduplicate_phi_incoming_blocks(Block* block, User* user) {
  if (const auto phi = cast<Phi>(user)) {
    // Make sure there is only one entry for this block in Phi instruction.
    // If there is more then one entry then make sure value is common and remove redundant ones.

    Value* common = nullptr;
    size_t count = 0;

    for (const auto incoming : *phi) {
      if (incoming.block != block) {
        continue;
      }

      count++;

      if (!common) {
        common = incoming.value;
      }

      verify(common == incoming.value, "Phi value isn't common for the same blocks");
    }

    for (size_t i = 1; i < count; ++i) {
      phi->remove_incoming(block);
    }
  }
}

Value::Value(Context* context, Value::Kind kind, Type* type)
    : context(context), kind(kind), type(type), uses(this) {
  verify(type->get_context() == context, "Context mismatch");
  context->increase_refcount();
}

Value::~Value() {
  verify(uses.get_size() == 0, "Cannot destroy value that has active users.");
  context->decrease_refcount();
}

void Value::set_display_index(size_t index) {
  verify(!type->is_void() && !type->is_function(),
         "Void or function values cannot have user index.");
  display_index = index;
}

bool Value::is_zero() const {
  const auto c = cast<Constant>(this);
  return c && c->get_constant_u() == 0;
}

bool Value::is_one() const {
  const auto c = cast<Constant>(this);
  return c && c->get_constant_u() == 1;
}

bool Value::is_all_ones() const {
  const auto c = cast<Constant>(this);
  return c && c->get_constant_i() == -1;
}

std::optional<uint64_t> Value::get_constant_u_opt() const {
  const auto c = cast<Constant>(this);
  return c ? std::optional(c->get_constant_u()) : std::nullopt;
}

std::optional<int64_t> Value::get_constant_i_opt() const {
  const auto c = cast<Constant>(this);
  return c ? std::optional(c->get_constant_i()) : std::nullopt;
}

bool Value::is_same_type_as(const Value* other) const {
  if (this == other) {
    return true;
  }

  if (get_type() != other->get_type()) {
    return false;
  }

  if (const auto other_function = cast<Function>(other)) {
    const auto function = cast<Function>(this);

    if (other_function->get_return_type() != function->get_return_type()) {
      return false;
    }

    if (other_function->get_parameter_count() != function->get_parameter_count()) {
      return false;
    }

    for (size_t i = 0; i < function->get_parameter_count(); ++i) {
      const auto t1 = function->get_parameter(i)->get_type();
      const auto t2 = other_function->get_parameter(i)->get_type();
      if (t1 != t2) {
        return false;
      }
    }
  }

  return true;
}

void Value::replace_uses_with(Value* new_value) {
  if (this == new_value) {
    return;
  }

  verify(!is_void(), "Cannot replace uses of void value");
  verify(is_same_type_as(new_value), "Cannot replace value with value of different type");

  const auto block = cast<Block>(new_value);

  while (!uses.is_empty()) {
    auto use = uses.get_first();
    User* user = use->get_user();
    user->set_operand(use->get_operand_index(), new_value);

    if (block) {
      deduplicate_phi_incoming_blocks(block, user);
    }
  }
}

void Value::replace_uses_with_constant(uint64_t constant) {
  replace_uses_with(get_type()->get_constant(constant));
}

void Value::replace_uses_with_undef() { replace_uses_with(get_type()->get_undef()); }

void Constant::constrain_constant(Type* type, uint64_t c, uint64_t* u, int64_t* i) {
  const auto bit_size = type->get_bit_size();
  const auto bit_mask = type->get_bit_mask();

  if (bit_size == 1) {
    const bool b = c != 0;

    if (u) {
      *u = b;
    }

    if (i) {
      *i = b;
    }
  } else {
    const auto masked = c & bit_mask;
    const auto sign_bit = (c & (1ull << (bit_size - 1))) != 0;

    if (u) {
      *u = masked;
    }

    if (i) {
      *i = int64_t(masked | (sign_bit ? ~bit_mask : 0));
    }
  }
}

Constant::Constant(Context* context, Type* type, uint64_t constant)
    : Value(context, Kind::Constant, type) {
  constrain_constant(type, constant, &constant_u, &constant_i);
}

uint64_t Constant::constrain_u(Type* type, uint64_t value) {
  uint64_t result;
  constrain_constant(type, value, &result, nullptr);
  return result;
}

int64_t Constant::constrain_i(Type* type, int64_t value) {
  int64_t result;
  constrain_constant(type, uint64_t(value), nullptr, &result);
  return result;
}

bool Value::is_used_only_by(const User* checked_user) const {
  return all_of(users(), [checked_user](const User& user) { return checked_user == &user; });
}

std::string Value::format() const { return "v" + std::to_string(display_index); }

std::string Constant::format() const {
  if (get_type()->is_i1()) {
    return constant_u == 0 ? "false" : "true";
  } else if (get_type()->is_pointer()) {
    return constant_u == 0 ? "null" : fmt::format("0x{:x}", constant_u);
  } else {
    return std::to_string(constant_i);
  }
}

std::string Undef::format() const { return "undef"; }