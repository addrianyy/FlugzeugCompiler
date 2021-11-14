#include "Value.hpp"
#include "Block.hpp"
#include "Instructions.hpp"

using namespace flugzeug;

void Value::add_use(const Use& use) {
  // TODO: Store uses in linked list.
  uses.push_back(use);

  if (use.user != this) {
    user_count_excluding_self++;
  }
}

void Value::remove_use(const Use& use) {
  // TODO: Store uses in linked list.
  const auto iterator = std::find(uses.begin(), uses.end(), use);

  verify(iterator != uses.end(), "Tried to remove invalid use.");

  // We don't care about the order so we move the element to the end and remove
  // it. This avoids copying vector's tail.
  std::iter_swap(iterator, uses.end() - 1);

  uses.erase(uses.end() - 1);

  if (use.user != this) {
    user_count_excluding_self--;
  }
}

Value::Value(Context* context, Value::Kind kind, Type* type)
    : context(context), kind(kind), type(type) {
  verify(type->get_context() == context, "Context mismatch");
  context->increase_refcount();
}

Value::~Value() {
  verify(uses.empty(), "Cannot destroy value that has active users.");
  context->decrease_refcount();
}

void Value::set_display_index(size_t index) {
  verify(!type->is_void(), "Void values cannot have user index.");
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

std::optional<uint64_t> Value::get_constant_u_opt() const {
  const auto c = cast<Constant>(this);
  return c ? std::optional(c->get_constant_u()) : std::nullopt;
}

std::optional<int64_t> Value::get_constant_i_opt() const {
  const auto c = cast<Constant>(this);
  return c ? std::optional(c->get_constant_i()) : std::nullopt;
}

void Value::replace_uses(Value* new_value) {
  if (this == new_value) {
    return;
  }

  verify(new_value->get_type() == get_type(), "Cannot replace value with value of different type");

  const auto block = cast<Block>(this);

  while (!uses.empty()) {
    const Use use = uses.back();
    use.user->set_operand(use.operand_index, new_value);

    if (block) {
      if (const auto phi = cast<Phi>(use.user)) {
        // Make sure there is only one entry for this block in Phi instruction.
        // If there is more then one entry then make sure value is common and remove redundant ones.

        Value* previous = nullptr;
        size_t count = 0;

        for (size_t i = 0; i < phi->get_incoming_count(); ++i) {
          const auto incoming = phi->get_incoming(i);
          if (incoming.block == block) {
            count++;

            if (!previous) {
              previous = incoming.value;
            }

            verify(previous == incoming.value, "Phi mismatched values");
          }
        }

        for (size_t i = 1; i < count; ++i) {
          phi->remove_incoming(block);
        }
      }
    }
  }
}

void Value::replace_uses_with_constant(uint64_t constant) {
  replace_uses(get_type()->get_constant(constant));
}

void Value::replace_uses_with_undef() { replace_uses(get_type()->get_undef()); }

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
    const auto sign_bit = (c & (1 << (bit_size - 1))) != 0;

    if (u) {
      *u = masked;
    }

    if (i) {
      *i = int64_t(masked | (sign_bit ? ~bit_mask : 0));
    }
  }
}

void Constant::initialize_constant(uint64_t c) {
  constrain_constant(get_type(), c, &constant_u, &constant_i);
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