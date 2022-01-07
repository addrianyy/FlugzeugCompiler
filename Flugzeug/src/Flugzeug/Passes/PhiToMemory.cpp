#include "PhiToMemory.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <vector>

using namespace flugzeug;

void PhiToMemory::convert_phi_to_memory(Phi* phi) {
  if (utils::simplify_phi(phi, true)) {
    return;
  }

  Context* context = phi->get_context();
  Block* entry_block = phi->get_function()->get_entry_block();
  Type* type = phi->get_type();

  const auto stackalloc = new StackAlloc(context, type);
  const auto load = new Load(context, stackalloc);

  for (size_t i = 0; i < phi->get_incoming_count(); ++i) {
    const auto incoming = phi->get_incoming(i);
    const auto store = new Store(context, stackalloc, incoming.value);

    store->insert_before(incoming.block->get_last_instruction());
  }

  entry_block->push_instruction_front(stackalloc);
  phi->replace_with_instruction_and_destroy(load);
}

bool PhiToMemory::run(Function* function) {
  bool did_something = false;

  for (Instruction& instruction : dont_invalidate_current(function->instructions())) {
    if (const auto phi = cast<Phi>(instruction)) {
      convert_phi_to_memory(phi);
      did_something = true;
    }
  }

  return did_something;
}