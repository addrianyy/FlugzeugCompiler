#include "PhiToMemory.hpp"
#include "Utils/SimplifyPhi.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <vector>

using namespace flugzeug;

static void convert_phi_to_memory(Phi* phi) {
  if (utils::simplify_phi(phi, true)) {
    return;
  }

  Context* context = phi->get_context();
  Block* entry_block = phi->get_function()->get_entry_block();
  Type* type = phi->get_type();

  const auto stackalloc = new StackAlloc(context, type);
  const auto load = new Load(context, stackalloc);

  for (const auto incoming : *phi) {
    const auto store = new Store(context, stackalloc, incoming.value);
    store->insert_before(incoming.block->get_last_instruction());
  }

  entry_block->push_instruction_front(stackalloc);
  phi->replace_with_instruction_and_destroy(load);
}

bool opt::PhiToMemory::run(Function* function) {
  bool did_something = false;

  for (Phi& phi : advance_early(function->instructions<Phi>())) {
    convert_phi_to_memory(&phi);
    did_something = true;
  }

  return did_something;
}