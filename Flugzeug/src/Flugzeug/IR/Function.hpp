#pragma once
#include "Block.hpp"
#include "Type.hpp"
#include "TypeFilteringIterator.hpp"
#include "Validator.hpp"
#include "Value.hpp"

#include <Flugzeug/Core/IntrusiveLinkedList.hpp>

#include <string_view>
#include <vector>

namespace flugzeug {

class Context;
class Module;

class Function : public Value, public IntrusiveNode<Function, Module> {
  DEFINE_INSTANCEOF(Value, Value::Kind::Function)

  friend class IntrusiveLinkedList<Block, Function>;
  friend class IntrusiveNode<Block, Function>;
  friend class Block;
  friend class Module;

  using BlockList = IntrusiveLinkedList<Block, Function>;

  template <typename TBlock, typename TInstruction> class InstructionIteratorInternal {
    TBlock* current_block;
    TInstruction* current_instruction;

    void get_instruction_for_current_block() {
      current_instruction = nullptr;

      while (current_block) {
        current_instruction = current_block->get_first_instruction();
        if (current_instruction) {
          break;
        }

        current_block = current_block->get_next();
      }
    }

  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = TInstruction;
    using pointer = value_type*;
    using reference = value_type&;

    explicit InstructionIteratorInternal(TBlock* current_block)
        : current_block(current_block), current_instruction(nullptr) {
      get_instruction_for_current_block();
    }

    InstructionIteratorInternal& operator++() {
      if (const auto next = current_instruction->get_next()) {
        current_instruction = next;
      } else {
        current_block = current_block->get_next();
        get_instruction_for_current_block();
      }

      return *this;
    }

    InstructionIteratorInternal operator++(int) {
      const auto before = *this;
      ++(*this);
      return before;
    }

    reference operator*() const { return *current_instruction; }
    pointer operator->() const { return current_instruction; }

    bool operator==(const InstructionIteratorInternal& rhs) const {
      return current_block == rhs.current_block && current_instruction == rhs.current_instruction;
    }
    bool operator!=(const InstructionIteratorInternal& rhs) const { return !(*this == rhs); }
  };

  Type* const return_type;
  const std::string name;
  std::vector<Parameter*> parameters;

  BlockList blocks;

  /// Always first block in the list.
  Block* entry_block = nullptr;

  size_t next_value_index = 0;
  size_t next_block_index = 0;

  size_t allocate_value_index() { return next_value_index++; }
  size_t allocate_block_index() { return next_block_index++; }

  BlockList& get_list() { return blocks; }

  void on_added_node(Block* block);
  void on_removed_node(Block* block);

  void print_prototype(IRPrinter& printer, bool end_line) const;
  void generate_dot_graph_source(std::ostream& output, bool colors) const;

  Function(Context* context, Type* return_type, std::string name,
           const std::vector<Type*>& arguments);

public:
  ~Function() override;

  ValidationResults validate(ValidationBehaviour behaviour) const;

  void print(IRPrinter& printer) const;
  void print() const;
  void debug_print() const;

  void generate_graph(const std::string& graph_path) const;
  void debug_graph() const;

#pragma region block_list
  Block* get_first_block() { return blocks.get_first(); }
  Block* get_last_block() { return blocks.get_last(); }

  const Block* get_first_block() const { return blocks.get_first(); }
  const Block* get_last_block() const { return blocks.get_last(); }

  size_t get_block_count() const { return blocks.get_size(); }
  bool is_empty() const { return blocks.is_empty(); }

  using const_iterator = BlockList::const_iterator;
  using iterator = BlockList::iterator;

  iterator begin() { return blocks.begin(); }
  iterator end() { return blocks.end(); }

  const_iterator begin() const { return blocks.begin(); }
  const_iterator end() const { return blocks.end(); }
#pragma endregion

  Module* get_module() { return get_owner(); }
  const Module* get_module() const { return get_owner(); }

  Type* get_return_type() const { return return_type; }
  std::string_view get_name() const { return name; }

  size_t get_parameter_count() const { return parameters.size(); }

  Parameter* get_parameter(size_t i) { return parameters[i]; }
  const Parameter* get_parameter(size_t i) const { return parameters[i]; }

  Block* get_entry_block() { return get_first_block(); }
  const Block* get_entry_block() const { return get_first_block(); }

  bool is_extern() const { return is_empty(); }

  void reassign_display_indices();

  Block* create_block();
  void insert_block(Block* block);

  void destroy();

  using InstructionIterator = InstructionIteratorInternal<Block, Instruction>;
  using ConstInstructionIterator = InstructionIteratorInternal<const Block, const Instruction>;

  template <typename TInstruction>
  using SpecificInstructionIterator = TypeFilteringIterator<TInstruction, InstructionIterator>;
  template <typename TInstruction>
  using ConstSpecificInstructionIterator =
    TypeFilteringIterator<const TInstruction, ConstInstructionIterator>;

  IteratorRange<InstructionIterator> instructions() {
    return {InstructionIterator(get_first_block()), InstructionIterator(nullptr)};
  }
  IteratorRange<ConstInstructionIterator> instructions() const {
    return {ConstInstructionIterator(get_first_block()), ConstInstructionIterator(nullptr)};
  }

  template <typename TInstruction>
  IteratorRange<SpecificInstructionIterator<TInstruction>> instructions() {
    return {SpecificInstructionIterator<TInstruction>(InstructionIterator(get_first_block())),
            SpecificInstructionIterator<TInstruction>(InstructionIterator(nullptr))};
  }
  template <typename TInstruction>
  IteratorRange<ConstSpecificInstructionIterator<TInstruction>> instructions() const {
    return {
      ConstSpecificInstructionIterator<TInstruction>(ConstInstructionIterator(get_first_block())),
      ConstSpecificInstructionIterator<TInstruction>(ConstInstructionIterator(nullptr))};
  }
};

}; // namespace flugzeug