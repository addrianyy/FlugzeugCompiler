#pragma once
#include "Context.hpp"
#include "Type.hpp"

#include <Core/Casting.hpp>
#include <Core/ClassTraits.hpp>

#include <string>
#include <vector>

namespace flugzeug {

class Context;
class User;

template <typename TUse, typename TUser> class UserIterator {
  TUse* current;

public:
  explicit UserIterator(TUse* current) : current(current) {}

  UserIterator& operator++() {
    current++;
    return *this;
  }

  TUser& operator*() const { return *current->user; }
  TUser* operator->() const { return current->user; }

  bool operator==(const UserIterator& rhs) const { return current == rhs.current; }
  bool operator!=(const UserIterator& rhs) const { return current != rhs.current; }
};

template <typename TValue, typename TUse, typename TUser> class ValueUsers {
  TValue* value;

public:
  explicit ValueUsers(TValue* value) : value(value) {}

  UserIterator<TUse, TUser> begin() { return UserIterator<TUse, TUser>(value->uses.data()); }
  UserIterator<TUse, TUser> end() {
    return UserIterator<TUse, TUser>(value->uses.data() + value->uses.size());
  }
};

class Value {
public:
  // clang-format off
  enum class Kind {
    Constant,
    Parameter,
    Undef,
    Block,
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

  struct Use {
    User* user = nullptr;
    size_t operand_index = 0;

    bool operator==(const Use& other) const {
      return user == other.user && operand_index == other.operand_index;
    }
  };

  friend class ValueUsers<Value, Use, User>;
  friend class ValueUsers<const Value, const Use, const User>;

  using NormalUsers = ValueUsers<Value, Use, User>;
  using ConstUsers = ValueUsers<const Value, const Use, const User>;

  const Kind kind;

  Type* const type;
  Context* const context;

  std::vector<Use> uses;

  size_t display_index = 0;

  void add_use(const Use& use);
  void remove_use(const Use& use);

protected:
  const std::vector<Use>& get_uses() { return uses; }

  Value(Context* context, Kind kind, Type* type);

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Value);

  virtual ~Value();

  Context* get_context() { return context; }
  const Context* get_context() const { return context; }

  Type* get_type() { return type; }
  const Type* get_type() const { return type; }

  Kind get_kind() const { return kind; }

  size_t get_display_index() const { return display_index; }
  void set_display_index(size_t index);

  bool is_void() const { return get_type()->is_void(); }

  void replace_uses(Value* new_value);
  void replace_uses_with_constant(uint64_t constant);

  bool is_zero() const;
  bool is_one() const;
  bool is_undef() const { return kind == Kind::Undef; }

  size_t get_user_count() const { return uses.size(); }
  bool is_used() const { return get_user_count() > 0; }
  bool is_used_once() const { return get_user_count() == 1; }

  size_t get_user_count_excluding_self();

  NormalUsers get_users() { return NormalUsers(this); }
  ConstUsers get_users() const { return ConstUsers(this); }

  virtual std::string format() const;
};

class Constant : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Constant)

  friend class Context;

  uint64_t constant_u = 0;
  int64_t constant_i = 0;

  void initialize_constant(uint64_t c);

  static void constrain_constant(Type* type, uint64_t c, uint64_t* u, int64_t* i);

  Constant(Context* context, Type* type, uint64_t constant) : Value(context, Kind::Constant, type) {
    initialize_constant(constant);
  }

public:
  uint64_t get_constant_u() const { return constant_u; }
  int64_t get_constant_i() const { return constant_i; }

  std::string format() const override;
};

class Parameter : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Parameter)

  friend class Function;

  explicit Parameter(Context* context, Type* type) : Value(context, Kind::Parameter, type) {}
};

class Undef : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Undef)

  friend class Context;

  explicit Undef(Context* context, Type* type) : Value(context, Kind::Undef, type) {}

public:
  std::string format() const override;
};

} // namespace flugzeug