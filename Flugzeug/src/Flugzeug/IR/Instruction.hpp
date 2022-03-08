#pragma once
#include "IRPrinter.hpp"
#include "User.hpp"

#include <Flugzeug/Core/HashCombine.hpp>
#include <Flugzeug/Core/IntrusiveLinkedList.hpp>

#include <unordered_set>

namespace flugzeug {

class Block;
class IRPrinter;
class DominatorTree;

using InstructionUniqueIdentifier = std::vector<uintptr_t>;

struct InstructionUniqueIdentifierHash {
  size_t operator()(const InstructionUniqueIdentifier& identifier) const {
    size_t hash = 0;

    for (const auto element : identifier) {
      hash_combine(hash, element);
    }

    return hash;
  }
};

template <typename TBlock> class BlockTargets {
  constexpr static size_t max_targets = 2;

  TBlock* targets[max_targets];
  size_t target_count = 0;

public:
  BlockTargets() = default;

  void insert(TBlock* block) {
    verify(target_count < max_targets, "Already filled.");

    targets[target_count++] = block;
  }

  TBlock** begin() { return targets; }
  TBlock** end() { return targets + target_count; }

  TBlock** begin() const { return targets; }
  TBlock** end() const { return targets + target_count; }

  size_t size() const { return target_count; }
  bool empty() const { return target_count == 0; }
};

class Instruction : public User, public IntrusiveNode<Instruction, Block> {
  DEFINE_INSTANCEOF_RANGE(Value, Value::Kind::InstructionBegin, Value::Kind::InstructionEnd)

  friend class Block;

  mutable size_t order_in_block = 0;

  size_t get_order_in_block() const;

protected:
  using User::User;

  static void print_value_compact(const Value* value, IRPrinter::LinePrinter& printer,
                                  const std::unordered_set<const Value*>& inlined_values,
                                  bool parens = true);

  virtual void print_instruction_internal(IRPrinter::LinePrinter& printer) const = 0;
  virtual void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const = 0;

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

  InstructionUniqueIdentifier calculate_unique_identifier() const;

  bool print_compact(IRPrinter& printer,
                     const std::unordered_set<const Value*>& inlined_values) const;

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

} // namespace flugzeug