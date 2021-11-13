#pragma once
#include "Instruction.hpp"

#include <Core/IntrusiveLinkedList.hpp>

#include <unordered_set>

namespace flugzeug {

class Function;

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
  friend class Function;

  using InstructionList = IntrusiveLinkedList<Instruction, Block>;

  InstructionList instructions;
  bool is_entry = false;

  InstructionList& get_list() { return instructions; }

  void on_added_node(Instruction* instruction);
  void on_removed_node(Instruction* instruction);

  void remove_all_references_in_phis();

public:
  explicit Block(Context* context)
      : Value(context, Value::Kind::Block, context->get_block_ty()), instructions(this) {}
  ~Block() override;

  void print(IRPrinter& printer) const;

#pragma region instruction_list
  Instruction* get_first_instruction() { return instructions.get_first(); }
  Instruction* get_last_instruction() { return instructions.get_last(); }

  const Instruction* get_first_instruction() const { return instructions.get_first(); }
  const Instruction* get_last_instruction() const { return instructions.get_last(); }

  size_t get_instruction_count() const { return instructions.get_size(); }
  bool is_empty() const { return instructions.is_empty(); }

  void push_instruction_front(Instruction* instruction) { instructions.push_front(instruction); }
  void push_instruction_back(Instruction* instruction) { instructions.push_back(instruction); }

  using const_iterator = InstructionList::const_iterator;
  using iterator = InstructionList::iterator;
  using const_reverse_iterator = InstructionList::const_reverse_iterator;
  using reverse_iterator = InstructionList::reverse_iterator;

  iterator begin() { return instructions.begin(); }
  iterator end() { return instructions.end(); }

  const_iterator begin() const { return instructions.begin(); }
  const_iterator end() const { return instructions.end(); }

  reverse_iterator rbegin() { return instructions.rbegin(); }
  reverse_iterator rend() { return instructions.rend(); }

  const_reverse_iterator rbegin() const { return instructions.rbegin(); }
  const_reverse_iterator rend() const { return instructions.rend(); }

  InstructionList ::ReversedRange reversed() { return instructions.reversed(); }
  InstructionList ::ReversedConstRange reversed() const { return instructions.reversed(); }
#pragma endregion

  bool is_entry_block() const { return is_entry; }

  Function* get_function() { return get_owner(); }
  const Function* get_function() const { return get_owner(); }

  std::string format() const override;

  void clear();
  void destroy();

  /// Remove `incoming` block from all Phis in this block.
  void remove_incoming_block_from_phis(Block* incoming);

  /// Call when branch from `this` to `to` was removed. This function will Phi incoming values in
  /// `to` when needed.
  void on_removed_branch_to(Block* to);

  bool has_successor(const Block* successor) const;
  bool has_predecessor(const Block* predecessor) const;

  BlockTargets<Block> get_successors();
  BlockTargets<const Block> get_successors() const;

  std::unordered_set<Block*> get_predecessors();
  std::unordered_set<const Block*> get_predecessors() const;

  std::vector<Block*> get_reachable_blocks(TraversalType traversal);
  std::vector<const Block*> get_reachable_blocks(TraversalType traversal) const;

  std::unordered_set<Block*> get_reachable_blocks_set(IncludeStart include_start);
  std::unordered_set<const Block*> get_reachable_blocks_set(IncludeStart include_start) const;
};

} // namespace flugzeug