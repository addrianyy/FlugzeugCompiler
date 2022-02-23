#pragma once
#include "Instruction.hpp"

#include <Flugzeug/Core/IntrusiveLinkedList.hpp>

#include <span>
#include <unordered_set>

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

class Block : public Value, public IntrusiveNode<Block, Function> {
  DEFINE_INSTANCEOF(Value, Value::Kind::Block)

  friend class IntrusiveLinkedList<Instruction, Block>;
  friend class IntrusiveNode<Instruction, Block>;
  friend class Instruction;
  friend class Function;
  friend class Value;

  using InstructionList = IntrusiveLinkedList<Instruction, Block>;

  InstructionList instruction_list;
  bool is_entry = false;

  mutable bool invalid_instruction_order = false;

  std::vector<Block*> predecessors_list;
  std::vector<Block*> predecessors_list_unique;

  InstructionList& get_list() { return instruction_list; }

  void on_added_node(Instruction* instruction);
  void on_removed_node(Instruction* instruction);

  void on_added_block_user(User* user);
  void on_removed_block_user(User* user);

  void add_predecessor(Block* predecessor);
  void remove_predecessor(Block* predecessor);

  void update_instruction_order() const;

  explicit Block(Context* context)
      : Value(context, Value::Kind::Block, context->get_block_ty()), instruction_list(this) {}

public:
  ~Block() override;

  std::unordered_set<const Value*> get_inlineable_values_created_in_block() const;

  void print(IRPrinter& printer, IRPrintingMethod method = IRPrintingMethod::Standard) const;
  void print(IRPrintingMethod method = IRPrintingMethod::Standard) const;
  void print_compact(IRPrinter& printer, const std::unordered_set<const Value*>& inlined) const;
  void debug_print() const;

#pragma region instruction_list
  Instruction* get_first_instruction() { return instruction_list.get_first(); }
  Instruction* get_last_instruction() { return instruction_list.get_last(); }

  const Instruction* get_first_instruction() const { return instruction_list.get_first(); }
  const Instruction* get_last_instruction() const { return instruction_list.get_last(); }

  size_t get_instruction_count() const { return instruction_list.get_size(); }
  bool is_empty() const { return instruction_list.is_empty(); }

  void push_instruction_front(Instruction* instruction) {
    instruction_list.push_front(instruction);
  }
  void push_instruction_back(Instruction* instruction) { instruction_list.push_back(instruction); }

  using const_iterator = InstructionList::const_iterator;
  using iterator = InstructionList::iterator;
  using const_reverse_iterator = InstructionList::const_reverse_iterator;
  using reverse_iterator = InstructionList::reverse_iterator;

  template <typename TInstruction>
  using SpecificInstructionIterator = TypeFilteringIterator<TInstruction, iterator>;
  template <typename TInstruction>
  using ConstSpecificInstructionIterator =
    TypeFilteringIterator<const TInstruction, const_iterator>;

  iterator begin() { return instruction_list.begin(); }
  iterator end() { return instruction_list.end(); }

  const_iterator begin() const { return instruction_list.begin(); }
  const_iterator end() const { return instruction_list.end(); }

  reverse_iterator rbegin() { return instruction_list.rbegin(); }
  reverse_iterator rend() { return instruction_list.rend(); }

  const_reverse_iterator rbegin() const { return instruction_list.rbegin(); }
  const_reverse_iterator rend() const { return instruction_list.rend(); }

  InstructionList::ReversedRange reversed() { return instruction_list.reversed(); }
  InstructionList::ReversedConstRange reversed() const { return instruction_list.reversed(); }

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

  Function* get_function() { return get_owner(); }
  const Function* get_function() const { return get_owner(); }

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

  Block* get_single_successor();
  const Block* get_single_successor() const;

  Block* get_single_predecessor();
  const Block* get_single_predecessor() const;

  BlockTargets<Block> successors();
  BlockTargets<const Block> successors() const;

  std::span<Block*> predecessors();
  std::span<const Block*> predecessors() const;

  std::unordered_set<Block*> predecessors_set();
  std::unordered_set<const Block*> predecessors_set() const;

  std::vector<Block*> get_reachable_blocks(TraversalType traversal);
  std::vector<const Block*> get_reachable_blocks(TraversalType traversal) const;

  std::unordered_set<Block*> get_reachable_blocks_set(IncludeStart include_start);
  std::unordered_set<const Block*> get_reachable_blocks_set(IncludeStart include_start) const;
};

template <typename TInstruction1, typename TInstruction2>
auto make_instruction_range(TInstruction1* begin, TInstruction2* end) {
  using Iterator =
    std::conditional_t<std::is_const_v<TInstruction1>, Block::const_iterator, Block::iterator>;

#if 1
  verify(begin == end || begin->is_before(end),
         "Begin must be before end for `make_instruction_range`");
#endif

  return IteratorRange(Iterator(begin), Iterator(end));
}

} // namespace flugzeug