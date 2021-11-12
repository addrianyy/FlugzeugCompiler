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

  const Kind kind;
  const Type type;

  Context* const context;

  std::vector<Use> uses;

  size_t display_index = 0;

  void add_use(const Use& use);
  void remove_use(const Use& use);

protected:
  const std::vector<Use>& get_uses() { return uses; }

  Value(Context* context, Kind kind, Type type);

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Value);

  virtual ~Value();

  Context* get_context() { return context; }
  const Context* get_context() const { return context; }

  Kind get_kind() const { return kind; }
  Type get_type() const { return type; }

  size_t get_display_index() const { return display_index; }
  void set_display_index(size_t index);

  bool is_void() const { return get_type().is_void(); }

  void replace_uses(Value* new_value);
  void replace_uses_with_constant(uint64_t constant);

  bool is_zero() const;
  bool is_one() const;

  virtual std::string format() const;
};

class Constant : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Constant)

  uint64_t constant_u = 0;
  int64_t constant_i = 0;

  void initialize_constant(uint64_t c);

public:
  static void constrain_constant(Type type, uint64_t c, uint64_t* u, int64_t* i);

  Constant(Context* context, Type type, uint64_t constant) : Value(context, Kind::Constant, type) {
    initialize_constant(constant);
  }

  uint64_t get_constant_u() const { return constant_u; }
  int64_t get_constant_i() const { return constant_i; }

  std::string format() const override;
};

class Parameter : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Parameter)

public:
  explicit Parameter(Context* context, Type type) : Value(context, Kind::Parameter, type) {}
};

class Undef : public Value {
  DEFINE_INSTANCEOF(Value, Value::Kind::Undef)

public:
  explicit Undef(Context* context, Type type) : Value(context, Kind::Undef, type) {}

  std::string format() const override;
};

} // namespace flugzeug