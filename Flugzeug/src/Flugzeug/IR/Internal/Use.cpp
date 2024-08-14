#include "Use.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace flugzeug;
using namespace flugzeug::detail;

#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
void flugzeug::detail::validate_use(const Value* used_value, const detail::Use* use) {
  verify(use->used_value() == used_value, "Use iterator was invalidated");
}
#endif

void ValueUses::add_use(Use* use) {
  verify(!use->next_ && !use->previous_, "This use is already inserted");

#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  verify(!use->used_value_, "Used value is already set.");
  use->used_value_ = value_;
#endif

  // Insert at the end of the list.
  if (first_ == nullptr) {
    use->previous_ = use->next_ = nullptr;
    first_ = last_ = use;
  } else {
    use->previous_ = last_;
    use->next_ = nullptr;

    last_->next_ = use;
    last_ = use;
  }

  size_++;
}

void ValueUses::remove_use(Use* use) {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  verify(use->used_value_ == value_, "Used value is invalid.");
  use->used_value_ = nullptr;
#endif

  if (use->previous_) {
    use->previous_->next_ = use->next_;
  } else {
    first_ = use->next_;
  }

  if (use->next_) {
    use->next_->previous_ = use->previous_;
  } else {
    last_ = use->previous_;
  }

  use->next_ = use->previous_ = nullptr;

  size_--;
}
