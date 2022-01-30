#pragma once
#include "Context.hpp"
#include "Type.hpp"
#include "TypeFilteringIterator.hpp"
#include "Use.hpp"

#include <Flugzeug/Core/Casting.hpp>
#include <Flugzeug/Core/ClassTraits.hpp>
#include <Flugzeug/Core/Iterator.hpp>

#include <optional>
#include <string>
#include <vector>

namespace flugzeug {

class Context;
class User;

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

  const Kind kind;

  Type* const type;
  Context* const context;

  ValueUses uses;
  size_t user_count_excluding_self = 0;

  size_t display_index = 0;

  void add_use(Use* use);
  void remove_use(Use* use);

protected:
  Value(Context* context, Kind kind, Type* type);

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Value);

  virtual ~Value();

  Kind get_kind() const { return kind; }

  Context* get_context() { return context; }
  const Context* get_context() const { return context; }

  Type* get_type() { return type; }
  const Type* get_type() const { return type; }

  size_t get_display_index() const { return display_index; }
  void set_display_index(size_t index);

  bool is_void() const { return get_type()->is_void(); }
  bool is_global() const {
    return kind == Kind::Undef || kind == Kind::Function || kind == Kind::Constant;
  }

  void replace_uses_with(Value* new_value);
  void replace_uses_with_constant(uint64_t constant);
  void replace_uses_with_undef();

  bool is_zero() const;
  bool is_one() const;
  bool is_all_ones() const;
  bool is_undef() const { return kind == Kind::Undef; }

  std::optional<uint64_t> get_constant_u_opt() const;
  std::optional<int64_t> get_constant_i_opt() const;

  size_t get_user_count() const { return uses.get_size(); }
  size_t get_user_count_excluding_self() const { return user_count_excluding_self; }

  bool is_used() const { return get_user_count_excluding_self() > 0; }
  bool is_used_once() const { return get_user_count_excluding_self() == 1; }

  virtual std::string format() const;

  using UserIterator = ValueUses::iterator;
  using ConstUserIterator = ValueUses::const_iterator;

  template <typename TUser> using SpecificUserIterator = TypeFilteringIterator<TUser, UserIterator>;
  template <typename TUser>
  using ConstSpecificUserIterator = TypeFilteringIterator<const TUser, ConstUserIterator>;

  IteratorRange<UserIterator> users() { return {uses.begin(), uses.end()}; }
  IteratorRange<ConstUserIterator> users() const { return {uses.begin(), uses.end()}; }

  template <typename TUser> IteratorRange<SpecificUserIterator<TUser>> users() {
    return {SpecificUserIterator<TUser>(uses.begin()), SpecificUserIterator<TUser>(uses.end())};
  }
  template <typename TUser> IteratorRange<ConstSpecificUserIterator<TUser>> users() const {
    return {ConstSpecificUserIterator<TUser>(uses.begin()),
            ConstSpecificUserIterator<TUser>(uses.end())};
  }
};

class Constant : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Constant)

  friend class Context;

  uint64_t constant_u = 0;
  int64_t constant_i = 0;

  static void constrain_constant(Type* type, uint64_t c, uint64_t* u, int64_t* i);

  Constant(Context* context, Type* type, uint64_t constant);

public:
  uint64_t get_constant_u() const { return constant_u; }
  int64_t get_constant_i() const { return constant_i; }

  std::string format() const override;
};

class Parameter : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Parameter)

  friend class Function;

  Parameter(Context* context, Type* type) : Value(context, Kind::Parameter, type) {}
};

class Undef : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Undef)

  friend class Context;

  Undef(Context* context, Type* type) : Value(context, Kind::Undef, type) {}

public:
  std::string format() const override;
};

} // namespace flugzeug