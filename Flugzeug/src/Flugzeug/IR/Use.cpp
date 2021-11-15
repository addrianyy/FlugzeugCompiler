#include "Use.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace flugzeug;

#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
void flugzeug::detail::validate_use(const Value* used_value, const Use* use) {
  verify(use->get_used_value() == used_value, "Use iterator was invalidated");
}
#endif

void ValueUses::add_use(Use* use) {
  verify(!use->next && !use->previous, "This use is already inserted");

#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  verify(!use->used_value, "Used value is already set.");
  use->used_value = value;
#endif

  // Insert at the end of the list.
  if (first == nullptr) {
    use->previous = use->next = nullptr;
    first = last = use;
  } else {
    use->previous = last;
    use->next = nullptr;

    last->next = use;
    last = use;
  }

  size++;
}

void ValueUses::remove_use(Use* use) {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  verify(use->used_value == value, "Used value is invalid.");
  use->used_value = nullptr;
#endif

  if (use->previous) {
    use->previous->next = use->next;
  } else {
    first = use->next;
  }

  if (use->next) {
    use->next->previous = use->previous;
  } else {
    last = use->previous;
  }

  use->next = use->previous = nullptr;

  size--;
}
