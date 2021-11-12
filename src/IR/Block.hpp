#pragma once
#include "Instruction.hpp"
#include <Core/IntrusiveLinkedList.hpp>
#include <unordered_set>

namespace flugzeug {

class Function;

class Block : public Value, public IntrusiveNode<Block, Function> {
  DEFINE_INSTANCEOF(Value, Value::Kind::Block)

  friend class IntrusiveLinkedList<Instruction, Block>;
  friend class IntrusiveNode<Instruction, Block>;
  friend class Function;

  IntrusiveLinkedList<Instruction, Block> instructions;
  bool is_entry = false;

  decltype(instructions)& get_list() { return instructions; }

  void on_added_node(Instruction* instruction);
  void on_removed_node(Instruction* instruction);

public:
  explicit Block(Context* context)
      : Value(context, Value::Kind::Block, Type::Kind::Block), instructions(this) {}
  ~Block() override;

#pragma region instruction_list
  Instruction* get_first() { return instructions.get_first(); }
  Instruction* get_last() { return instructions.get_last(); }

  const Instruction* get_first() const { return instructions.get_first(); }
  const Instruction* get_last() const { return instructions.get_last(); }

  size_t get_instruction_count() const { return instructions.get_size(); }
  bool is_empty() const { return instructions.is_empty(); }

  void push_front(Instruction* instruction) { instructions.push_front(instruction); }
  void push_back(Instruction* instruction) { instructions.push_back(instruction); }

  using const_iterator = IntrusiveLinkedList<Instruction, Block>::const_iterator;
  using iterator = IntrusiveLinkedList<Instruction, Block>::iterator;

  iterator begin() { return instructions.begin(); }
  iterator end() { return instructions.end(); }

  const_iterator begin() const { return instructions.begin(); }
  const_iterator end() const { return instructions.end(); }
#pragma endregion

  void print(IRPrinter& printer) const;

  bool is_entry_block() const { return is_entry; }

  Function* get_function() { return get_owner(); }
  const Function* get_function() const { return get_owner(); }

  std::string format() const override;

  void remove_phi_incoming();
  void destroy();

  BlockTargets<Block> get_successors();
  BlockTargets<const Block> get_successors() const;

  std::unordered_set<Block*> get_predecessors();

  std::vector<Block*> traverse_dfs();
};

} // namespace flugzeug