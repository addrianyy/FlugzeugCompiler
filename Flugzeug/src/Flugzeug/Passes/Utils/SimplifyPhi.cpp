#include "SimplifyPhi.hpp"
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

bool utils::simplify_phi(Phi* phi, bool destroy_unused) {
  if (const auto incoming = phi->get_single_incoming_value()) {
    phi->replace_uses_with_and_destroy(incoming);
    return true;
  } else if (phi->is_empty() || (!phi->is_used() && destroy_unused)) {
    phi->destroy();
    return true;
  }

  return false;
}