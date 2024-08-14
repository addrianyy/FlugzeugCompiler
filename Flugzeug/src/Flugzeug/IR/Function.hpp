#pragma once
#include "Block.hpp"
#include "Internal/TypeFilteringIterator.hpp"
#include "Type.hpp"
#include "Validator.hpp"
#include "Value.hpp"

#include <Flugzeug/Core/IntrusiveLinkedList.hpp>

#include <string_view>
#include <vector>

namespace flugzeug {

class Context;
class Module;

class Function final : public Value, public ll::IntrusiveNode<Function, Module> {
  DEFINE_INSTANCEOF(Value, Value::Kind::Function)

  friend class ll::IntrusiveLinkedList<Block, Function>;
  friend class ll::IntrusiveNode<Block, Function>;
  friend class Block;
  friend class Module;

  using BlockList = ll::IntrusiveLinkedList<Block, Function>;

  template <typename TBlock, typename TInstruction>
  class InstructionIteratorInternal {
    TBlock* current_block;
    TInstruction* current_instruction;

    void visit_block() {
      current_instruction = nullptr;

      while (current_block) {
        current_instruction = current_block->first_instruction();
        if (current_instruction) {
          break;
        }

        current_block = current_block->next();
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
      visit_block();
    }

    InstructionIteratorInternal& operator++() {
      if (const auto next = current_instruction->next()) {
        current_instruction = next;
      } else {
        current_block = current_block->next();
        visit_block();
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

  Type* const return_type_;
  const std::string name_;
  std::vector<Parameter*> parameters_;

  /// Entry block is always first block in the list.
  BlockList blocks_;

  size_t next_value_index = 0;
  size_t next_block_index = 0;

  size_t allocate_value_index() { return next_value_index++; }
  size_t allocate_block_index() { return next_block_index++; }

  BlockList& intrusive_list() { return blocks_; }

  void on_added_node(Block* block);
  void on_removed_node(Block* block);

  void print_prototype(IRPrinter& printer, bool end_line) const;
  void generate_dot_graph_source(std::ostream& output,
                                 bool colors,
                                 IRPrintingMethod method = IRPrintingMethod::Standard) const;

  void insert_block(Block* block);

  Function(Context* context,
           Type* return_type,
           std::string name,
           const std::vector<Type*>& arguments);

 public:
  ~Function() override;

  ValidationResults validate(ValidationBehaviour behaviour) const;

  void print(IRPrinter& printer, IRPrintingMethod method = IRPrintingMethod::Standard) const;
  void print(IRPrintingMethod method = IRPrintingMethod::Standard) const;
  void debug_print() const;

  void generate_graph(const std::string& graph_path,
                      IRPrintingMethod method = IRPrintingMethod::Standard) const;
  void debug_graph() const;

#pragma region block_list
  Block* first_block() { return blocks_.first(); }
  Block* last_block() { return blocks_.last(); }

  const Block* first_block() const { return blocks_.first(); }
  const Block* last_block() const { return blocks_.last(); }

  size_t block_count() const { return blocks_.size(); }
  bool empty() const { return blocks_.empty(); }

  using const_iterator = BlockList::const_iterator;
  using iterator = BlockList::iterator;

  iterator begin() { return blocks_.begin(); }
  iterator end() { return blocks_.end(); }

  const_iterator begin() const { return blocks_.begin(); }
  const_iterator end() const { return blocks_.end(); }
#pragma endregion

  Module* module() { return owner(); }
  const Module* module() const { return owner(); }

  Type* return_type() const { return return_type_; }
  std::string_view name() const { return name_; }

  size_t parameter_count() const { return parameters_.size(); }

  Parameter* parameter(size_t i) { return parameters_[i]; }
  const Parameter* parameter(size_t i) const { return parameters_[i]; }

  Block* entry_block() { return first_block(); }
  const Block* entry_block() const { return first_block(); }

  bool is_extern() const { return empty(); }
  bool is_local() const { return !empty(); }

  void reassign_display_indices();

  Block* create_block();

  void destroy();

  std::string format() const override;

  using InstructionIterator = InstructionIteratorInternal<Block, Instruction>;
  using ConstInstructionIterator = InstructionIteratorInternal<const Block, const Instruction>;

  template <typename TInstruction>
  using SpecificInstructionIterator =
    detail::TypeFilteringIterator<TInstruction, InstructionIterator>;
  template <typename TInstruction>
  using ConstSpecificInstructionIterator =
    detail::TypeFilteringIterator<const TInstruction, ConstInstructionIterator>;

  IteratorRange<InstructionIterator> instructions() {
    return {InstructionIterator(first_block()), InstructionIterator(nullptr)};
  }
  IteratorRange<ConstInstructionIterator> instructions() const {
    return {ConstInstructionIterator(first_block()), ConstInstructionIterator(nullptr)};
  }

  template <typename TInstruction>
  IteratorRange<SpecificInstructionIterator<TInstruction>> instructions() {
    return {SpecificInstructionIterator<TInstruction>(InstructionIterator(first_block())),
            SpecificInstructionIterator<TInstruction>(InstructionIterator(nullptr))};
  }
  template <typename TInstruction>
  IteratorRange<ConstSpecificInstructionIterator<TInstruction>> instructions() const {
    return {ConstSpecificInstructionIterator<TInstruction>(ConstInstructionIterator(first_block())),
            ConstSpecificInstructionIterator<TInstruction>(ConstInstructionIterator(nullptr))};
  }
};

};  // namespace flugzeug