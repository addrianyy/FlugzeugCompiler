#pragma once
#include "IRPrinter.hpp"
#include "User.hpp"

#include <Flugzeug/Core/HashCombine.hpp>
#include <Flugzeug/Core/IntrusiveLinkedList.hpp>
#include <Flugzeug/Core/StaticVector.hpp>

#include <unordered_set>

namespace flugzeug {

class Block;
class IRPrinter;
class DominatorTree;

template <typename TBlock>
using BlockTargets = StaticVector<TBlock*, 2>;

class Instruction : public User, public IntrusiveNode<Instruction, Block> {
  DEFINE_INSTANCEOF_RANGE(Value, Value::Kind::InstructionBegin, Value::Kind::InstructionEnd)

  friend class Block;
  friend class Function;

  mutable size_t order_in_block = 0;

  size_t get_order_in_block() const;

 protected:
  using User::User;

  static void print_value_compact(const Value* value,
                                  IRPrinter::LinePrinter& printer,
                                  const std::unordered_set<const Value*>& inlined_values,
                                  bool parens = true);

  virtual void print_instruction_internal(IRPrinter::LinePrinter& printer) const = 0;
  virtual void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const = 0;

  bool print_compact(IRPrinter& printer,
                     const std::unordered_set<const Value*>& inlined_values) const;

 public:
  using IntrusiveNode::insert_after;
  using IntrusiveNode::insert_before;
  using IntrusiveNode::move_after;
  using IntrusiveNode::move_before;
  using IntrusiveNode::move_to_back;
  using IntrusiveNode::move_to_front;
  using IntrusiveNode::push_back;
  using IntrusiveNode::push_front;
  using IntrusiveNode::unlink;

  virtual Instruction* clone() = 0;

  void print(IRPrinter& printer) const;
  void print() const;
  void debug_print() const;

  Block* get_block() { return get_owner(); }
  const Block* get_block() const { return get_owner(); }

  Function* get_function();
  const Function* get_function() const;

  void destroy();
  bool destroy_if_unused();

  void replace_with_instruction_and_destroy(Instruction* instruction);
  void replace_uses_with_and_destroy(Value* new_value);
  void replace_uses_with_constant_and_destroy(uint64_t constant);

  /// If value is an uninserted instruction then replace it.
  /// If value is constant/undef/inserted instruction then replace all uses.
  /// In both cases current instruction will be destroyed.
  void replace_instruction_or_uses_and_destroy(Value* new_value);

  BlockTargets<Block> targets();
  BlockTargets<const Block> targets() const;

  bool is_before(const Instruction* other) const;
  bool is_after(const Instruction* other) const;

  bool is_dominated_by(const Block* block, const DominatorTree& dominator_tree) const;

  bool dominates(const Instruction* other, const DominatorTree& dominator_tree) const;
  bool is_dominated_by(const Instruction* other, const DominatorTree& dominator_tree) const;

  bool is_volatile() const;
  bool is_branching() const;
  bool is_terminator() const;
};

}  // namespace flugzeug