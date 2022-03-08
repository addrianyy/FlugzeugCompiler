#pragma once
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/Passes/Analysis/PointerAliasing.hpp>

namespace flugzeug::opt::memory {

bool eliminate_known_loads_local(Function* function,
                                 const analysis::PointerAliasing& alias_analysis);
bool eliminate_known_loads_global(Function* function, const DominatorTree& dominator_tree,
                                  const analysis::PointerAliasing& alias_analysis);

} // namespace flugzeug::opt::memory