#pragma once
#include "Error.hpp"

#include <type_traits>

namespace flugzeug {

namespace detail {

template <typename T>
class EarlyAdvancingIterator {
  T current;
  T next;

  bool has_next = false;

  void on_dereference() {
    if (!has_next) {
      has_next = true;
      next = current;
      ++next;
    }
  }

 public:
  using value_type = typename T::value_type;
  using pointer = value_type*;
  using reference = value_type&;

  explicit EarlyAdvancingIterator(T current) : current(current), next(current) {}

  EarlyAdvancingIterator& operator++() {
    verify(has_next, "Invalid use of early-advancing iterator");

    current = next;
    has_next = false;

    return *this;
  }

  reference operator*() {
    on_dereference();
    return *current;
  }

  pointer operator->() {
    on_dereference();
    return current.operator->();
  }

  bool operator==(const EarlyAdvancingIterator& rhs) const { return current == rhs.current; }
  bool operator!=(const EarlyAdvancingIterator& rhs) const { return current != rhs.current; }
};

}  // namespace detail

template <typename T>
class IteratorRange {
  T begin_it;
  T end_it;

 public:
  using iterator = T;

  IteratorRange(T begin_it, T end_it) : begin_it(begin_it), end_it(end_it) {}

  T begin() const { return begin_it; }
  T end() const { return end_it; }
};

template <typename T, typename std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
inline IteratorRange<detail::EarlyAdvancingIterator<typename T::iterator>> advance_early(T object) {
  return IteratorRange(detail::EarlyAdvancingIterator(object.begin()),
                       detail::EarlyAdvancingIterator(object.end()));
}

template <typename T, typename std::enable_if_t<!std::is_trivially_copyable_v<T>, int> = 0>
inline IteratorRange<detail::EarlyAdvancingIterator<typename T::iterator>> advance_early(
  T& object) {
  return IteratorRange(detail::EarlyAdvancingIterator(object.begin()),
                       detail::EarlyAdvancingIterator(object.end()));
}

template <typename T>
inline IteratorRange<detail::EarlyAdvancingIterator<typename T::const_iterator>> advance_early(
  const T& object) {
  return IteratorRange(detail::EarlyAdvancingIterator(object.begin()),
                       detail::EarlyAdvancingIterator(object.end()));
}

template <typename T, typename std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
inline IteratorRange<typename T::reverse_iterator> reversed(T object) {
  return IteratorRange(object.rbegin(), object.rend());
}

template <typename T, typename std::enable_if_t<!std::is_trivially_copyable_v<T>, int> = 0>
inline IteratorRange<typename T::reverse_iterator> reversed(T& object) {
  return IteratorRange(object.rbegin(), object.rend());
}

template <typename T>
inline IteratorRange<typename T::const_reverse_iterator> reversed(const T& object) {
  return IteratorRange(object.rbegin(), object.rend());
}

template <typename R, typename Fn>
bool all_of(R&& range, Fn predicate) {
  for (auto&& element : range) {
    if (!predicate(element)) {
      return false;
    }
  }

  return true;
}

template <typename R, typename Fn>
bool any_of(R&& range, Fn predicate) {
  for (auto&& element : range) {
    if (predicate(element)) {
      return true;
    }
  }

  return false;
}

template <typename R, typename Fn>
bool none_of(R&& range, Fn predicate) {
  for (auto&& element : range) {
    if (predicate(element)) {
      return false;
    }
  }

  return true;
}

}  // namespace flugzeug