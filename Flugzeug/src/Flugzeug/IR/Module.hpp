#pragma once
#include "Function.hpp"

namespace flugzeug {

class Context;

class Module {
  friend class ll::IntrusiveLinkedList<Function, Module>;
  friend class ll::IntrusiveNode<Function, Module>;
  friend class Context;

  using FunctionList = ll::IntrusiveLinkedList<Function, Module>;

  template <typename TIterator, bool Extern>
  class FunctionFilteringIterator {
    TIterator iterator;

    void skip_non_matching_functions() {
      while (true) {
        const auto function = iterator.operator->();
        if (!function || function->is_extern() == Extern) {
          break;
        }
        ++iterator;
      }
    }

   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = typename TIterator::value_type;
    using pointer = typename TIterator::pointer;
    using reference = typename TIterator::reference;

    explicit FunctionFilteringIterator(TIterator iterator) : iterator(iterator) {
      skip_non_matching_functions();
    }

    FunctionFilteringIterator& operator++() {
      ++iterator;
      skip_non_matching_functions();
      return *this;
    }

    FunctionFilteringIterator operator++(int) {
      const auto before = *this;
      ++(*this);
      return before;
    }

    reference operator*() const { return *iterator; }
    pointer operator->() { return iterator->operator->(); }

    bool operator==(const FunctionFilteringIterator& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const FunctionFilteringIterator& rhs) const { return iterator != rhs.iterator; }
  };

  Context* context_;
  FunctionList function_list_;

  std::unordered_map<std::string_view, Function*> function_map;

  FunctionList& intrusive_list() { return function_list_; }

  void on_added_node(Function* function);
  void on_removed_node(Function* function);

  explicit Module(Context* context);

 public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Module)

  ~Module();

  void print(IRPrinter& printer, IRPrintingMethod method = IRPrintingMethod::Standard) const;
  void print(IRPrintingMethod method = IRPrintingMethod::Standard) const;

  void destroy();

  size_t function_count() const { return function_list_.size(); }
  bool empty() const { return function_list_.empty(); }

  Context* context() { return context_; }
  const Context* context() const { return context_; }

  std::unordered_map<const Function*, ValidationResults> validate(
    ValidationBehaviour behaviour) const;

  Function* create_function(Type* return_type,
                            std::string name,
                            const std::vector<Type*>& arguments);

  Function* find_function(std::string_view name);
  const Function* find_function(std::string_view name) const;

  using const_iterator = FunctionList::const_iterator;
  using iterator = FunctionList::iterator;

  using LocalFunctionIterator = FunctionFilteringIterator<iterator, false>;
  using ConstLocalFunctionIterator = FunctionFilteringIterator<const_iterator, false>;

  using ExternFunctionIterator = FunctionFilteringIterator<iterator, true>;
  using ConstExternFunctionIterator = FunctionFilteringIterator<const_iterator, true>;

  iterator begin() { return function_list_.begin(); }
  iterator end() { return function_list_.end(); }

  const_iterator begin() const { return function_list_.begin(); }
  const_iterator end() const { return function_list_.end(); }

  IteratorRange<LocalFunctionIterator> local_functions() {
    return {LocalFunctionIterator(begin()), LocalFunctionIterator(end())};
  }
  IteratorRange<ConstLocalFunctionIterator> local_functions() const {
    return {ConstLocalFunctionIterator(begin()), ConstLocalFunctionIterator(end())};
  }

  IteratorRange<ExternFunctionIterator> extern_functions() {
    return {ExternFunctionIterator(begin()), ExternFunctionIterator(end())};
  }
  IteratorRange<ConstExternFunctionIterator> extern_functions() const {
    return {ConstExternFunctionIterator(begin()), ConstExternFunctionIterator(end())};
  }
};

}  // namespace flugzeug