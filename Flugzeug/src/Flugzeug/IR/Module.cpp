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
  verify(function_list.empty(), "Cannot remove non-empty module.");
  context->decrease_refcount();
}

void Module::print(IRPrinter& printer, IRPrintingMethod method) const {
  for (const Function& function : *this) {
    function.print(printer, method);

    if (function.next()) {
      printer.newline();
    }
  }
}

void Module::print(IRPrintingMethod method) const {
  ConsolePrinter printer(ConsolePrinter::Variant::ColorfulIfSupported);
  print(printer, method);
}

void Module::destroy() {
  for (Function& function : advance_early(*this)) {
    // Remove all calls to this function.
    for (Instruction& instruction : advance_early(function.users<Instruction>())) {
      instruction.destroy();
    }

    // Destroy function.
    function.destroy();
  }

  delete this;
}

std::unordered_map<const Function*, ValidationResults> Module::validate(
  ValidationBehaviour behaviour) const {
  std::unordered_map<const Function*, ValidationResults> all_results;
  size_t total_erors = 0;

  const auto new_behaviour = behaviour == ValidationBehaviour::ErrorsAreFatal
                               ? ValidationBehaviour::ErrorsToStdout
                               : behaviour;

  for (const Function& function : local_functions()) {
    auto results = function.validate(new_behaviour);
    if (results.has_errors()) {
      total_erors += results.get_errors().size();
      all_results.insert({&function, std::move(results)});
    }
  }

  if (behaviour == ValidationBehaviour::ErrorsAreFatal && total_erors > 0) {
    if (total_erors == 1) {
      fatal_error("Encountered 1 validation error in the module.");
    } else {
      fatal_error("Encountered {} validation errors in the module.", total_erors);
    }
  }

  return all_results;
}

Function* Module::create_function(Type* return_type,
                                  std::string name,
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