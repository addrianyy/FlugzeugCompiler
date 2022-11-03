#pragma once

namespace flugzeug {

class Phi;

namespace utils {

/// Destroy Phi if it has no incoming values.
/// Replace Phi if it has single incoming value.
/// Destroy Phi if it is unused and `removed_unused` is true.
bool simplify_phi(Phi* phi, bool destroy_unused);

}  // namespace utils

}  // namespace flugzeug