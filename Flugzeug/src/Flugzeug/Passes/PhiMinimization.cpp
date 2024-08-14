#include "PhiMinimization.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Analysis/SCC.hpp"

using namespace flugzeug;

bool minimize_phis(const std::unordered_set<Phi*>& phis,
                   const std::unordered_map<Phi*, std::vector<Value*>>& phi_graph);

static bool process_scc(const std::vector<Phi*>& scc,
                        const std::unordered_map<Phi*, std::vector<Value*>>& phi_graph) {
  // If there is only one Phi in the SCC than it is already in the simplest posssible form.
  if (scc.size() == 1) {
    return false;
  }

  std::unordered_set<Phi*> inner;
  std::unordered_set<Value*> outer;

  // Go through all Phis in the SCC.
  for (Phi* phi : scc) {
    bool is_inner = true;

    // Get all operands outside SCC that this Phi references.
    for (Value* operand : phi_graph.find(phi)->second) {
      if (std::find(scc.begin(), scc.end(), operand) == scc.end()) {
        outer.insert(operand);
        is_inner = false;
      }
    }

    // If this Phi references only values inside the SCC then we have sub-SCC that we will
    // try to optimize later.
    if (is_inner) {
      inner.insert(phi);
    }
  }

  if (outer.empty()) {
    return false;
  } else if (outer.size() > 1) {
    // Phis in the SCC reference each other and more than one other value.
    // Try minimizing SCC of Phis which don't reference other values.
    return minimize_phis(inner, phi_graph);
  } else {
    // All Phis in the SCC reference each other and one other value.
    // We can optimize Phis to just reference that other value.
    const auto value = *outer.begin();

    for (Phi* phi : scc) {
      phi->replace_uses_with_and_destroy(value);
    }

    return true;
  }
}

bool minimize_phis(const std::unordered_set<Phi*>& phis,
                   const std::unordered_map<Phi*, std::vector<Value*>>& phi_graph) {
  const auto sccs = analysis::calculate_sccs<Phi*, false>(
    phis,
    [&phi_graph](Phi* phi) -> const std::vector<Value*>& { return phi_graph.find(phi)->second; });

  bool minimized = false;

  for (const auto& scc : sccs) {
    minimized |= process_scc(scc, phi_graph);
  }

  return minimized;
}

bool opt::PhiMinimization::run(Function* function) {
  std::unordered_map<Phi*, std::vector<Value*>> phi_graph;
  std::unordered_set<Phi*> phis;

  // Connect Phis which reference each other.
  for (Phi& phi : function->instructions<Phi>()) {
    std::vector<Value*> phi_operands;
    phi_operands.reserve(phi.incoming_count());

    for (const auto incoming : phi) {
      phi_operands.push_back(incoming.value);
    }

    phi_graph.insert({&phi, std::move(phi_operands)});
    phis.insert(&phi);
  }

  return minimize_phis(phis, phi_graph);
}
