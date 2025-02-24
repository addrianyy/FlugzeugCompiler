#pragma once
#include <Flugzeug/Core/IntrusiveLinkedList.hpp>
#include <span>
#include <unordered_set>

#include "Instruction.hpp"

namespace flugzeug {

class Function;
class DominatorTree;

enum class TraversalType {
  BFS_WithStart,
  DFS_WithStart,
  BFS_WithoutStart,
  DFS_WithoutStart,
};

enum class IncludeStart {
  Yes,
  No,
};

class Block final : public Value, public ll::IntrusiveNode<Block, Function> {
  DEFINE_INSTANCEOF(Value, Value::Kind::Block)

  friend class ll::IntrusiveLinkedList<Instruction, Block>;
  friend class ll::IntrusiveNode<Instruction, Block>;
  friend class Instruction;
  friend class Function;
  friend class Value;

  using InstructionList = ll::IntrusiveLinkedList<Instruction, Block>;

  InstructionList instruction_list;
  bool is_entry = false;

  mutable bool invalid_instruction_order = false;

  std::vector<Block*> predecessors_list;
  std::vector<Block*> predecessors_list_unique;

  InstructionList& intrusive_list() { return instruction_list; }

  void on_added_node(Instruction* instruction);
  void on_removed_node(Instruction* instruction);

  void on_added_block_user(User* user);
  void on_removed_block_user(User* user);

  void add_predecessor(Block* predecessor);
  void remove_predecessor(Block* predecessor);

  void update_instruction_order() const;

  std::unordered_set<const Value*> get_inlinable_values() const;

  explicit Block(Context* context)
      : Value(context, Value::Kind::Block, context->block_ty()), instruction_list(this) {}

 public:
  ~Block() override;

  void print(IRPrinter& printer, IRPrintingMethod method = IRPrintingMethod::Standard) const;
  void print(IRPrintingMethod method = IRPrintingMethod::Standard) const;
  void debug_print() const;

#pragma region instruction_list
  Instruction* first_instruction() { return instruction_list.first(); }
  Instruction* last_instruction() { return instruction_list.last(); }

  const Instruction* first_instruction() const { return instruction_list.first(); }
  const Instruction* last_instruction() const { return instruction_list.last(); }

  size_t instruction_count() const { return instruction_list.size(); }
  bool empty() const { return instruction_list.empty(); }

  void push_instruction_front(Instruction* instruction) {
    instruction_list.push_front(instruction);
  }
  void push_instruction_back(Instruction* instruction) { instruction_list.push_back(instruction); }

  using const_iterator = InstructionList::const_iterator;
  using iterator = InstructionList::iterator;
  using const_reverse_iterator = InstructionList::const_reverse_iterator;
  using reverse_iterator = InstructionList::reverse_iterator;

  template <typename TInstruction>
  using SpecificInstructionIterator = detail::TypeFilteringIterator<TInstruction, iterator>;
  template <typename TInstruction>
  using ConstSpecificInstructionIterator =
    detail::TypeFilteringIterator<const TInstruction, const_iterator>;

  iterator begin() { return instruction_list.begin(); }
  iterator end() { return instruction_list.end(); }

  const_iterator begin() const { return instruction_list.begin(); }
  const_iterator end() const { return instruction_list.end(); }

  reverse_iterator rbegin() { return instruction_list.rbegin(); }
  reverse_iterator rend() { return instruction_list.rend(); }

  const_reverse_iterator rbegin() const { return instruction_list.rbegin(); }
  const_reverse_iterator rend() const { return instruction_list.rend(); }

  template <typename TInstruction>
  IteratorRange<SpecificInstructionIterator<TInstruction>> instructions() {
    return {SpecificInstructionIterator<TInstruction>(begin()),
            SpecificInstructionIterator<TInstruction>(end())};
  }
  template <typename TInstruction>
  IteratorRange<ConstSpecificInstructionIterator<TInstruction>> instructions() const {
    return {ConstSpecificInstructionIterator<TInstruction>(begin()),
            ConstSpecificInstructionIterator<TInstruction>(end())};
  }
#pragma endregion

  bool is_entry_block() const { return is_entry; }

  Function* function() { return owner(); }
  const Function* function() const { return owner(); }

  std::string format() const override;

  void clear();
  void destroy();
  void clear_and_destroy();

  void replace_incoming_blocks_in_phis(const Block* old_incoming, Block* new_incoming);

  /// Remove `incoming` block from all Phis in this block.
  void remove_incoming_block_from_phis(const Block* incoming, bool destroy_empty_phis);

  /// Call when branch from `this` to `to` was removed. This function will Phi incoming values in
  /// `to` when needed.
  void on_removed_branch_to(Block* to, bool destroy_empty_phis) const;

  bool is_terminated() const;

  bool dominates(const Block* other, const DominatorTree& dominator_tree) const;
  bool is_dominated_by(const Block* other, const DominatorTree& dominator_tree) const;

  bool has_successor(const Block* successor) const;
  bool has_predecessor(const Block* predecessor) const;

  Block* single_successor();
  const Block* single_successor() const;

  Block* single_predecessor();
  const Block* single_predecessor() const;

  BlockTargets<Block> successors();
  BlockTargets<const Block> successors() const;

  std::span<Block*> predecessors();
  std::span<const Block*> predecessors() const;

  std::unordered_set<Block*> predecessors_set();
  std::unordered_set<const Block*> predecessors_set() const;

  std::vector<Block*> reachable_blocks(TraversalType traversal);
  std::vector<const Block*> reachable_blocks(TraversalType traversal) const;

  std::unordered_set<Block*> reachable_blocks_set(IncludeStart include_start);
  std::unordered_set<const Block*> reachable_blocks_set(IncludeStart include_start) const;
};

template <typename TInstruction1, typename TInstruction2>
auto instruction_range(TInstruction1* begin, TInstruction2* end) {
  using Iterator =
    std::conditional_t<std::is_const_v<TInstruction1>, Block::const_iterator, Block::iterator>;

#if 1
  if (end) {
    verify(begin == end || begin->is_before(end),
           "Begin must be before end for `instruction_range`");
  }
#endif

  return IteratorRange(Iterator(begin), Iterator(end));
}
template <typename TInstruction1>
auto instruction_range(TInstruction1* begin, std::nullptr_t end) {
  using Iterator =
    std::conditional_t<std::is_const_v<TInstruction1>, Block::const_iterator, Block::iterator>;

  return IteratorRange(Iterator(begin), Iterator(nullptr));
}

}  // namespace flugzeug