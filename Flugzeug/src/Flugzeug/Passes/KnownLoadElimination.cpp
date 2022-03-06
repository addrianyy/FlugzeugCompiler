#include "KnownLoadElimination.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include "Analysis/PointerAliasing.hpp"

using namespace flugzeug;

static bool eliminate_known_loads_local(Function* function) {
  bool did_something = false;

  analysis::PointerAliasing alias_analysis(function);
  std::unordered_map<Value*, Store*> stores;

  for (Block& block : *function) {
    stores.clear();

    for (Instruction& instruction : dont_invalidate_current(block)) {
      if (const auto store = cast<Store>(instruction)) {
        // The newest known value for the pointer is now defined by this store.
        stores[store->get_ptr()] = store;
        continue;
      }

      if (const auto load = cast<Load>(instruction)) {
        const auto it = stores.find(load->get_ptr());
        if (it == stores.end()) {
          continue;
        }

        // Make sure that nothing inbetween can modify the pointer.
        const auto store = it->second;
        const auto stored_to_inbetween = alias_analysis.is_pointer_accessed_inbetween(
          load->get_ptr(), store->get_next(), load, analysis::PointerAliasing::AccessType::Store);

        if (!stored_to_inbetween) {
          // Alias load instruction with recently stored value.
          load->replace_uses_with_and_destroy(store->get_val());
          did_something = true;
        }
      }
    }
  }

  return did_something;
}

static bool eliminate_known_loads_global(Function* function) { fatal_error("TODO"); }

bool opt::KnownLoadElimination::run(Function* function, OptimizationLocality locality) {
  switch (locality) {
  case OptimizationLocality::BlockLocal:
    return eliminate_known_loads_local(function);

  case OptimizationLocality::Global:
    return eliminate_known_loads_global(function);

  default:
    unreachable();
  }
}