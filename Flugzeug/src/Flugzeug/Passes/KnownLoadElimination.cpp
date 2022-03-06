#include "KnownLoadElimination.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>

using namespace flugzeug;

static bool eliminate_known_loads_local(Function* function) { return false; }

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