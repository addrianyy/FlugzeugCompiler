#pragma once
#include "Use.hpp"
#include "Value.hpp"

#include <Flugzeug/Core/ClassTraits.hpp>
#include <Flugzeug/Core/Iterator.hpp>

#include <vector>

namespace flugzeug {

class User : public Value {
  DEFINE_INSTANCEOF_RANGE(Value, Value::Kind::UserBegin, Value::Kind::UserEnd)

  friend class Value;

  constexpr static size_t static_use_count = 3;
  Use static_uses[static_use_count]{};

  std::vector<Value*> used_operands;
  std::vector<Use*> uses_for_operands;

  template <typename TOperand> class OperandIteratorInternal {
    TOperand* const* current;

  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = TOperand;
    using pointer = value_type*;
    using reference = value_type&;

    explicit OperandIteratorInternal(TOperand* const* current) : current(current) {}

    OperandIteratorInternal& operator++() {
      current++;
      return *this;
    }

    OperandIteratorInternal operator++(int) {
      const auto before = *this;
      ++(*this);
      return before;
    }

    reference operator*() const { return **current; }
    pointer operator->() const { return *current; }

    bool operator==(const OperandIteratorInternal& rhs) const { return current == rhs.current; }
    bool operator!=(const OperandIteratorInternal& rhs) const { return current != rhs.current; }
  };

protected:
  using Value::Value;

  void remove_phi_incoming_helper(size_t incoming_index);

  void adjust_uses_count(size_t count);
  void reserve_operands(size_t count);

  void set_operand_count(size_t count);
  void grow_operand_count(size_t grow);

public:
  ~User() override;

  size_t get_operand_count() const;

  Value* get_operand(size_t index);
  const Value* get_operand(size_t index) const;

  void set_operand(size_t index, Value* operand);

  bool uses_value(Value* value) const;
  void replace_operands(Value* old_value, Value* new_value);

  using OperandIterator = OperandIteratorInternal<Value>;
  using ConstOperandIterator = OperandIteratorInternal<const Value>;

  IteratorRange<OperandIterator> operands() {
    return {OperandIterator(used_operands.data()),
            OperandIterator(used_operands.data() + used_operands.size())};
  }
  IteratorRange<ConstOperandIterator> operands() const {
    return {ConstOperandIterator(used_operands.data()),
            ConstOperandIterator(used_operands.data() + used_operands.size())};
  }
};

} // namespace flugzeug