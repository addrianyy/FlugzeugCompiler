#pragma once
#include <Core/ClassTraits.hpp>
#include <IR/Value.hpp>

#include <vector>

namespace flugzeug {

class User : public Value {
  DEFINE_INSTANCEOF_RANGE(Value, Value::Kind::UserBegin, Value::Kind::UserEnd)

  friend class Value;

  std::vector<Value*> operands;

protected:
  using Value::Value;

  ~User() override;

  size_t get_operand_count() const;

  Value* get_operand(size_t index);
  const Value* get_operand(size_t index) const;

  void set_operand_count(size_t count);
  void grow_operand_count(size_t grow);
  void set_operand(size_t index, Value* operand);
};

} // namespace flugzeug