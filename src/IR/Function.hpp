#pragma once
#include "Block.hpp"
#include "Type.hpp"
#include <Core/IntrusiveLinkedList.hpp>

#include <string_view>
#include <vector>

namespace flugzeug {

class Context;

class Function {
  friend class IntrusiveLinkedList<Block, Function>;
  friend class IntrusiveNode<Block, Function>;
  friend class Block;

  Context* const context;

  Type* return_type;
  const std::string name;

  std::vector<Parameter*> parameters;

  IntrusiveLinkedList<Block, Function> blocks;
  Block* entry_block = nullptr;

  size_t next_value_index = 0;
  size_t next_block_index = 0;

  size_t allocate_value_index() { return next_value_index++; }
  size_t allocate_block_index() { return next_block_index++; }

  decltype(blocks)& get_list() { return blocks; }

  void on_added_node(Block* block);
  void on_removed_node(Block* block);

public:
  Function(Context* context, Type* return_type, std::string name,
           const std::vector<Type*>& arguments);
  ~Function();

#pragma region block_list
  Block* get_first() { return blocks.get_first(); }
  Block* get_last() { return blocks.get_last(); }

  const Block* get_first() const { return blocks.get_first(); }
  const Block* get_last() const { return blocks.get_last(); }

  size_t get_block_count() const { return blocks.get_size(); }
  bool is_empty() const { return blocks.is_empty(); }

  using const_iterator = IntrusiveLinkedList<Block, Function>::const_iterator;
  using iterator = IntrusiveLinkedList<Block, Function>::iterator;

  iterator begin() { return blocks.begin(); }
  iterator end() { return blocks.end(); }

  const_iterator begin() const { return blocks.begin(); }
  const_iterator end() const { return blocks.end(); }
#pragma endregion

  Context* get_context() { return context; }
  const Context* get_context() const { return context; }

  Type* get_return_type() const { return return_type; }
  std::string_view get_name() const { return name; }

  size_t get_parameter_count() const { return parameters.size(); }

  Parameter* get_parameter(size_t i) { return parameters[i]; }
  const Parameter* get_parameter(size_t i) const { return parameters[i]; }

  Block* get_entry_block() { return entry_block; }
  const Block* get_entry_block() const { return entry_block; }

  bool is_extern() const { return is_empty(); }

  void reassign_display_indices();

  Block* create_block();
  void insert_block(Block* block);
  void set_entry_block(Block* block);

  void print(IRPrinter& printer) const;

  void destroy();
};

}; // namespace flugzeug