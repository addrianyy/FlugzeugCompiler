#include "Module.hpp"
#include "ConsolePrinter.hpp"

using namespace flugzeug;

void Module::on_added_node(Function* function) {
  const auto name = function->get_name();
  verify(function_map.insert({name, function}).second,
         "Function with name {} already exists in the module", name);
}

void Module::on_removed_node(Function* function) {
  const auto name = function->get_name();
  verify(function_map.erase(name) > 0, "Cannot find function with name {} in the module.", name);
}

Module::Module(Context* context) : context(context), function_list(this) {
  context->increase_refcount();
}

Module::~Module() {
  verify(function_list.is_empty(), "Cannot remove non-empty module.");
  context->decrease_refcount();
}

void Module::print(IRPrinter& printer) const {
  for (const Function& function : *this) {
    function.print(printer);

    if (function.get_next()) {
      printer.newline();
    }
  }
}

void Module::print() const {
  ConsolePrinter printer(ConsolePrinter::Variant::ColorfulIfSupported);
  print(printer);
}

void Module::destroy() {
  for (Function& function : dont_invalidate_current(*this)) {
    // Remove all calls to this function.
    for (Instruction& instruction : dont_invalidate_current(function.users<Instruction>())) {
      instruction.destroy();
    }

    // Destroy function.
    function.destroy();
  }

  delete this;
}

Function* Module::create_function(Type* return_type, std::string name,
                                  const std::vector<Type*>& arguments) {
  auto function = new Function(context, return_type, std::move(name), arguments);
  function_list.push_back(function);
  return function;
}

Function* Module::get_function(std::string_view name) {
  const auto it = function_map.find(name);
  if (it != function_map.end()) {
    return it->second;
  }
  return nullptr;
}

const Function* Module::get_function(std::string_view name) const {
  const auto it = function_map.find(name);
  if (it != function_map.end()) {
    return it->second;
  }
  return nullptr;
}