#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename Class> class ClassFilteringPattern {
  Class** bind_value;

public:
  explicit ClassFilteringPattern(Class** bind_value) : bind_value(bind_value) {}

  template <typename T> bool match(T* m_value) {
    const auto value = cast<Class>(m_value);
    if (!value) {
      return false;
    }

    if (bind_value) {
      *bind_value = value;
    }

    return true;
  }
};

template <bool Unsigned> class ConstantExtractionPattern {
public:
  using Type = std::conditional_t<Unsigned, uint64_t, int64_t>;

private:
  Type& bind_value;

public:
  explicit ConstantExtractionPattern(Type& bind_value) : bind_value(bind_value) {}

  template <typename T> bool match(T* m_value) {
    const auto constant = cast<Constant>(m_value);
    if (!constant) {
      return false;
    }

    if constexpr (Unsigned) {
      bind_value = constant->get_constant_u();
    } else {
      bind_value = constant->get_constant_i();
    }

    return true;
  }
};

} // namespace detail

inline auto value() { return detail::ClassFilteringPattern<Value>(nullptr); }
template <typename T> inline auto value(T*& v) { return detail::ClassFilteringPattern<T>(&v); }

inline auto constant() { return detail::ClassFilteringPattern<Constant>(nullptr); }
inline auto constant(Constant*& v) { return detail::ClassFilteringPattern<Constant>(&v); }

inline auto undef() { return detail::ClassFilteringPattern<Undef>(nullptr); }
inline auto undef(Undef*& v) { return detail::ClassFilteringPattern<Undef>(&v); }

inline auto constant_u(uint64_t& constant) {
  return detail::ConstantExtractionPattern<true>(constant);
}
inline auto constant_i(int64_t& constant) {
  return detail::ConstantExtractionPattern<false>(constant);
}

} // namespace flugzeug::pat