#pragma once
#include "Context.hpp"
#include "Internal/TypeFilteringIterator.hpp"
#include "Internal/Use.hpp"
#include "Type.hpp"

#include <Flugzeug/Core/Casting.hpp>
#include <Flugzeug/Core/ClassTraits.hpp>
#include <Flugzeug/Core/Error.hpp>
#include <Flugzeug/Core/Iterator.hpp>

#include <optional>
#include <string>
#include <vector>

namespace flugzeug {

class Context;
class User;
class Block;

class Value {
 public:
  // clang-format off
  enum class Kind {
    Constant,
    Parameter,
    Undef,
    Block,
    Function,
    UserBegin,
      InstructionBegin,
        UnaryInstr,
        BinaryInstr,
        IntCompare,
        Load,
        Store,
        Call,
        Branch,
        CondBranch,
        StackAlloc,
        Ret,
        Offset,
        Cast,
        Select,
        Phi,
      InstructionEnd,
    UserEnd,
  };
  // clang-format on

 private:
  friend class User;

  const Kind kind_;

  Type* const type_;
  Context* const context_;

  detail::ValueUses uses;
  size_t user_count_excluding_self_ = 0;

  size_t display_index_ = 0;

  static Block* cast_to_block(Value* value);
  static void set_user_operand(User* user, size_t operand_index, Value* value);

  void add_use(detail::Use* use);
  void remove_use(detail::Use* use);

  bool is_phi() const;
  void deduplicate_phi_incoming_blocks(Block* block, User* user);

 protected:
  Value(Context* context, Kind kind, Type* type);

 public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Value);

  virtual ~Value();

  Kind kind() const { return kind_; }

  Context* context() { return context_; }
  const Context* context() const { return context_; }

  Type* type() const { return type_; }

  size_t display_index() const { return display_index_; }
  void set_display_index(size_t index);

  bool is_void() const { return type()->is_void(); }
  bool is_global() const {
    return kind_ == Kind::Undef || kind_ == Kind::Function || kind_ == Kind::Constant;
  }

  bool is_same_type_as(const Value* other) const;

  void replace_uses_with(Value* new_value);
  void replace_uses_with_constant(uint64_t constant);
  void replace_uses_with_undef();

  template <typename Fn>
  void replace_uses_with_predicated(Value* new_value, Fn&& predicate) {
    if (this == new_value) {
      return;
    }

    verify(!is_void(), "Cannot replace uses of void value");
    verify(is_same_type_as(new_value), "Cannot replace value with value of different type");

    const auto block = cast<Block>(new_value);

    auto current_use = uses.first();
    while (current_use) {
      auto next_use = current_use->next();

      User* user = current_use->user();
      if (predicate(user)) {
        set_user_operand(user, current_use->operand_index(), new_value);

        if (block) {
          deduplicate_phi_incoming_blocks(block, user);
        }
      }

      current_use = next_use;
    }
  }

  bool is_zero() const;
  bool is_one() const;
  bool is_all_ones() const;
  bool is_undef() const { return kind_ == Kind::Undef; }

  std::optional<uint64_t> constant_u_opt() const;
  std::optional<int64_t> constant_i_opt() const;

  size_t user_count() const { return uses.size(); }
  size_t user_count_excluding_self() const { return user_count_excluding_self_; }

  bool is_used() const { return user_count_excluding_self() > 0; }
  bool is_used_only_by(const User* user) const;

  virtual std::string format() const;

  using UserIterator = detail::ValueUses::iterator;
  using ConstUserIterator = detail::ValueUses::const_iterator;

  template <typename TUser>
  using SpecificUserIterator = detail::TypeFilteringIterator<TUser, UserIterator>;
  template <typename TUser>
  using ConstSpecificUserIterator = detail::TypeFilteringIterator<const TUser, ConstUserIterator>;

  IteratorRange<UserIterator> users() { return {uses.begin(), uses.end()}; }
  IteratorRange<ConstUserIterator> users() const { return {uses.begin(), uses.end()}; }

  template <typename TUser>
  IteratorRange<SpecificUserIterator<TUser>> users() {
    return {SpecificUserIterator<TUser>(uses.begin()), SpecificUserIterator<TUser>(uses.end())};
  }
  template <typename TUser>
  IteratorRange<ConstSpecificUserIterator<TUser>> users() const {
    return {ConstSpecificUserIterator<TUser>(uses.begin()),
            ConstSpecificUserIterator<TUser>(uses.end())};
  }
};

class Constant final : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Constant)

  friend class Context;

  uint64_t constant_u = 0;
  int64_t constant_i = 0;

  static void constrain_constant(Type* type, uint64_t c, uint64_t* u, int64_t* i);

  Constant(Context* context, Type* type, uint64_t constant);

 public:
  static uint64_t constrain_u(Type* type, uint64_t value);
  static int64_t constrain_i(Type* type, int64_t value);

  uint64_t value_u() const { return constant_u; }
  int64_t value_i() const { return constant_i; }

  std::string format() const override;
};

class Parameter final : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Parameter)

  friend class Function;

  Parameter(Context* context, Type* type) : Value(context, Kind::Parameter, type) {}
};

class Undef final : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Undef)

  friend class Context;

  Undef(Context* context, Type* type) : Value(context, Kind::Undef, type) {}

 public:
  std::string format() const override;
};

}  // namespace flugzeug