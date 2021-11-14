#include "Use.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace flugzeug;

void flugzeug::detail::validate_use(const Use* use) {
  if (use) {
    verify(use->is_valid(), "Accessed invalid Use");
  }
}

void ValueUses::add_use(Use* use) {
  verify(!use->next && !use->previous && !use->valid, "This use is already inserted");

  use->valid = true;

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
  verify(use->valid, "This Use is not yet inserted");

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

  use->valid = false;
  use->next = use->previous = nullptr;

  size--;
}