#pragma once

template <typename T> class NonInvalidatingIterator {
  T current;
  T next;

  bool has_next = false;

  void on_dereference() {
    if (!has_next) {
      next = current.get_next_iterator();
      has_next = true;
    }
  }

public:
  explicit NonInvalidatingIterator(T current) : current(current) {}

  NonInvalidatingIterator& operator++() {
    if (has_next) {
      current = next;
      has_next = false;
    } else {
      current = current.get_next_iterator();
    }

    return *this;
  }

  typename T::Item& operator*() {
    on_dereference();
    return *current;
  }

  typename T::Item* operator->() {
    on_dereference();
    return current.operator->();
  }

  bool operator==(const NonInvalidatingIterator& rhs) const { return current == rhs.current; }
  bool operator!=(const NonInvalidatingIterator& rhs) const { return current != rhs.current; }
};

template <typename T> class NonInvalidatingWrapper {
  T begin_it;
  T end_it;

public:
  NonInvalidatingWrapper(T begin_it, T end_it) : begin_it(begin_it), end_it(end_it) {}

  NonInvalidatingIterator<T> begin() { return NonInvalidatingIterator<T>(begin_it); }
  NonInvalidatingIterator<T> end() { return NonInvalidatingIterator<T>(end_it); }
};

template <typename T>
inline NonInvalidatingWrapper<typename T::iterator> dont_invalidate_current(T& object) {
  return NonInvalidatingWrapper<typename T::iterator>{object.begin(), object.end()};
}

template <typename T>
inline NonInvalidatingWrapper<typename T::const_iterator> dont_invalidate_current(const T& object) {
  return NonInvalidatingWrapper<typename T::const_iterator>{object.begin(), object.end()};
}