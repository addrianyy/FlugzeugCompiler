#pragma once
#include "Function.hpp"

namespace flugzeug {

class Context;

class Module {
  friend class IntrusiveLinkedList<Function, Module>;
  friend class IntrusiveNode<Function, Module>;
  friend class Context;

  using FunctionList = IntrusiveLinkedList<Function, Module>;

  Context* context;
  FunctionList function_list;

  std::unordered_map<std::string_view, Function*> function_map;

  FunctionList& get_list() { return function_list; }

  void on_added_node(Function* function);
  void on_removed_node(Function* function);

  explicit Module(Context* context);

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(Module)

  ~Module();

  void destroy();

  Context* get_context() { return context; }
  const Context* get_context() const { return context; }

  Function* create_function(Type* return_type, std::string name,
                            const std::vector<Type*>& arguments);

  Function* get_function(std::string_view name);
  const Function* get_function(std::string_view name) const;

  using const_iterator = FunctionList::const_iterator;
  using iterator = FunctionList::iterator;

  iterator begin() { return function_list.begin(); }
  iterator end() { return function_list.end(); }

  const_iterator begin() const { return function_list.begin(); }
  const_iterator end() const { return function_list.end(); }
};

} // namespace flugzeug