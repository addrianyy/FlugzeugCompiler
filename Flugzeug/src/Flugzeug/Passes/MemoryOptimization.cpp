#include "MemoryOptimization.hpp"
#include "Analysis/PointerAliasing.hpp"
#include "Memory/DeadStoreElimination.hpp"
#include "Memory/KnownLoadElimination.hpp"

using namespace flugzeug;

bool opt::MemoryOptimization::run(Function* function, opt::OptimizationLocality locality) {
  analysis::PointerAliasing alias_analysis(function);

  switch (locality) {
    case OptimizationLocality::BlockLocal:
      return memory::eliminate_dead_stores_local(function, alias_analysis) |
             memory::eliminate_known_loads_local(function, alias_analysis);

    case OptimizationLocality::Global: {
      const DominatorTree dominator_tree(function);

      return memory::eliminate_dead_stores_global(function, dominator_tree, alias_analysis) |
             memory::eliminate_known_loads_global(function, dominator_tree, alias_analysis);
    }

    default:
      unreachable();
  }
}
