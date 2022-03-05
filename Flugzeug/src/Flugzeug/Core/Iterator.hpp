#pragma once
#include <type_traits>

namespace detail {

template <typename T> class NonInvalidatingIterator {
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

  explicit NonInvalidatingIterator(T current) : current(current), next(current) {}

  NonInvalidatingIterator& operator++() {
    if (has_next) {
      current = next;
      has_next = false;
    } else {
      ++current;
    }

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

  bool operator==(const NonInvalidatingIterator& rhs) const { return current == rhs.current; }
  bool operator!=(const NonInvalidatingIterator& rhs) const { return current != rhs.current; }
};

} // namespace detail

template <typename T> class IteratorRange {
  T begin_it;
  T end_it;

public:
  using iterator = T;

  IteratorRange(T begin_it, T end_it) : begin_it(begin_it), end_it(end_it) {}

  T begin() const { return begin_it; }
  T end() const { return end_it; }
};

template <typename T, typename std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
inline IteratorRange<detail::NonInvalidatingIterator<typename T::iterator>>
dont_invalidate_current(T object) {
  return IteratorRange(detail::NonInvalidatingIterator(object.begin()),
                       detail::NonInvalidatingIterator(object.end()));
}

template <typename T, typename std::enable_if_t<!std::is_trivially_copyable_v<T>, int> = 0>
inline IteratorRange<detail::NonInvalidatingIterator<typename T::iterator>>
dont_invalidate_current(T& object) {
  return IteratorRange(detail::NonInvalidatingIterator(object.begin()),
                       detail::NonInvalidatingIterator(object.end()));
}

template <typename T>
inline IteratorRange<detail::NonInvalidatingIterator<typename T::const_iterator>>
dont_invalidate_current(const T& object) {
  return IteratorRange(detail::NonInvalidatingIterator(object.begin()),
                       detail::NonInvalidatingIterator(object.end()));
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