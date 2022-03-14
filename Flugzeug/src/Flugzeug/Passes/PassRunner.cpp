#include "PassRunner.hpp"

#include <Flugzeug/IR/Function.hpp>

using namespace flugzeug;

void FunctionPassRunner::on_finished_optimization(Function* function) {
  function->reassign_display_indices();
}

void FunctionPassRunner::validate() const {
  function->validate(ValidationBehaviour::ErrorsAreFatal);
}