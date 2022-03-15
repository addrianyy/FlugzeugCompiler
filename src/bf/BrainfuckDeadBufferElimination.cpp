#include "BrainfuckDeadBufferElimination.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/IR/Module.hpp>

using namespace flugzeug;

bool bf::BrainfuckDeadBufferElimination::run(Function* function) {
  bool did_something = false;

  const auto zero_buffer = function->get_module()->get_function("zero_buffer");
  if (!zero_buffer || zero_buffer->get_parameter_count() != 1) {
    return false;
  }

  for (Call& call : advance_early(zero_buffer->users<Call>())) {
    if (call.get_function() != function) {
      continue;
    }

    const auto buffer = call.get_arg(0);
    if (!buffer) {
      continue;
    }

    if (buffer->get_user_count() <= 1) {
      call.destroy();

      if (const auto instruction = cast<Instruction>(buffer)) {
        instruction->destroy_if_unused();
      }

      did_something = true;
    }
  }

  return did_something;
}