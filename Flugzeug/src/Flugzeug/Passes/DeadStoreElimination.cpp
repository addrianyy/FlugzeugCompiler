#include "DeadStoreElimination.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>

using namespace flugzeug;

static bool eliminate_dead_stores_local(Function* function) { return false; }

static bool eliminate_dead_stores_global(Function* function) { fatal_error("TODO"); }

bool opt::DeadStoreElimination::run(Function* function, OptimizationLocality locality) {
  switch (locality) {
  case OptimizationLocality::BlockLocal:
    return eliminate_dead_stores_local(function);

  case OptimizationLocality::Global:
    return eliminate_dead_stores_global(function);

  default:
    unreachable();
  }
}
