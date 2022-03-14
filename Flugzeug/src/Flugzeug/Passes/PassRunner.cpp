#include "PassRunner.hpp"

#include <Flugzeug/IR/Function.hpp>

using namespace flugzeug;

void FunctionPassRunner::validate() const {
  function->validate(ValidationBehaviour::ErrorsAreFatal);
}
