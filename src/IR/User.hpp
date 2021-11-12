#pragma once
#include <Core/ClassTraits.hpp>
#include <IR/Value.hpp>

#include <vector>

namespace flugzeug {

template <typename TOperand> class OperandIterator {
  TOperand** current;

public:
  explicit OperandIterator(TOperand** current) : current(current) {}

  OperandIterator& operator++() {
    current++;
    return *this;
  }

  TOperand& operator*() const { return **current; }
  TOperand* operator->() const { return *current; }

  bool operator==(const OperandIterator& rhs) const { return current == rhs.current; }
  bool operator!=(const OperandIterator& rhs) const { return current != rhs.current; }
};

template <typename TUser, typename TOperand> class UserOperands {
  TUser* user;

public:
  explicit UserOperands(TUser* user) : user(user) {}

  OperandIterator<TOperand> begin() { return OperandIterator<TOperand>(user->operands.data()); }
  OperandIterator<TOperand> end() {
    return OperandIterator<TOperand>(user->operands.data() + user->operands.size());
  }
};

class User : public Value {
  DEFINE_INSTANCEOF_RANGE(Value, Value::Kind::UserBegin, Value::Kind::UserEnd)

  friend class Value;
  friend class UserOperands<User, Value>;
  friend class UserOperands<const User, const Value>;

  using NormalOperands = UserOperands<User, Value>;
  using ConstOperands = UserOperands<const User, const Value>;

  std::vector<Value*> operands;

protected:
  using Value::Value;

  ~User() override;

  void set_operand_count(size_t count);
  void grow_operand_count(size_t grow);

public:
  size_t get_operand_count() const;

  Value* get_operand(size_t index);
  const Value* get_operand(size_t index) const;

  void set_operand(size_t index, Value* operand);

public:
  NormalOperands get_operands() { return NormalOperands(this); }
  ConstOperands get_operands() const { return ConstOperands(this); }
};

} // namespace flugzeug