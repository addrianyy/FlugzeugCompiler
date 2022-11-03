#pragma once
#include <Flugzeug/Core/Casting.hpp>
#include <iterator>

namespace flugzeug::detail {

template <typename T, typename TUnderlyingIterator>
class TypeFilteringIterator {
  TUnderlyingIterator underlying;

  void move_to_required_type() {
    while (true) {
      const auto object = underlying.operator->();
      if (!object || cast<T>(object)) {
        break;
      }

      ++underlying;
    }
  }

 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = T;
  using pointer = value_type*;
  using reference = value_type&;

  explicit TypeFilteringIterator(TUnderlyingIterator underlying) : underlying(underlying) {
    move_to_required_type();
  }

  TypeFilteringIterator& operator++() {
    ++underlying;
    move_to_required_type();
    return *this;
  }

  TypeFilteringIterator operator++(int) {
    const auto before = *this;
    ++(*this);
    return before;
  }

  reference operator*() const { return *cast<T>(*underlying); }
  pointer operator->() const { return cast<T>(underlying->operator->()); }

  bool operator==(const TypeFilteringIterator& rhs) const { return underlying == rhs.underlying; }
  bool operator!=(const TypeFilteringIterator& rhs) const { return !(*this == rhs); }
};

}  // namespace flugzeug::detail