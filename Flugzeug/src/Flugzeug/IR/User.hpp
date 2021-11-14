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

  std::vector<Value*> operands;
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

  void set_operand_count(size_t count);
  void grow_operand_count(size_t grow);

public:
  ~User() override;

  size_t get_operand_count() const;

  Value* get_operand(size_t index);
  const Value* get_operand(size_t index) const;

  void set_operand(size_t index, Value* operand);

  using OperandIterator = OperandIteratorInternal<Value>;
  using ConstOperandIterator = OperandIteratorInternal<const Value>;

  IteratorRange<OperandIterator> get_operands() {
    return IteratorRange(OperandIterator(operands.data()),
                         OperandIterator(operands.data() + operands.size()));
  }
  IteratorRange<ConstOperandIterator> get_operands() const {
    return IteratorRange(ConstOperandIterator(operands.data()),
                         ConstOperandIterator(operands.data() + operands.size()));
  }
};

} // namespace flugzeug