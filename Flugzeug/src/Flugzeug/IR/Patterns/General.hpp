#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename Class>
class ValueBindingPattern {
  Class** bind_value;

 public:
  explicit ValueBindingPattern(Class** bind_value) : bind_value(bind_value) {}

  template <typename T>
  bool match(T* m_value) {
    const auto value = flugzeug::cast<std::remove_cv_t<Class>>(m_value);
    if (!value) {
      return false;
    }

    if (bind_value) {
      *bind_value = value;
    }

    return true;
  }
};

template <bool Unsigned>
class ConstantBindingPattern {
 public:
  using Type = std::conditional_t<Unsigned, uint64_t, int64_t>;

 private:
  Type& bind_constant;

 public:
  explicit ConstantBindingPattern(Type& bind_value) : bind_constant(bind_value) {}

  template <typename T>
  bool match(T* m_value) {
    const auto constant = flugzeug::cast<Constant>(m_value);
    if (!constant) {
      return false;
    }

    if constexpr (Unsigned) {
      bind_constant = constant->value_u();
    } else {
      bind_constant = constant->value_i();
    }

    return true;
  }
};

template <typename TValue>
class ExactValuePattern {
  const TValue* exact_value;

 public:
  explicit ExactValuePattern(const TValue* exact_value) : exact_value(exact_value) {}

  template <typename T>
  bool match(T* m_value) {
    const auto value = flugzeug::cast<std::remove_cv_t<TValue>>(m_value);
    if (!value) {
      return false;
    }

    return value == exact_value;
  }
};

template <typename TValue>
class ExactValueRefPattern {
  TValue*& exact_value_ref;

 public:
  explicit ExactValueRefPattern(TValue*& exact_value_ref) : exact_value_ref(exact_value_ref) {}

  template <typename T>
  bool match(T* m_value) {
    const auto value = flugzeug::cast<std::remove_cv_t<TValue>>(m_value);
    if (!value) {
      return false;
    }

    return value == exact_value_ref;
  }
};

template <bool Unsigned>
class ExactConstantPattern {
 public:
  using Type = std::conditional_t<Unsigned, uint64_t, int64_t>;

 private:
  Type exact_constant;

 public:
  explicit ExactConstantPattern(Type exact_constant) : exact_constant(exact_constant) {}

  template <typename T>
  bool match(T* m_value) {
    const auto constant = flugzeug::cast<Constant>(m_value);
    if (!constant) {
      return false;
    }

    if constexpr (Unsigned) {
      return constant->value_u() == exact_constant;
    } else {
      return constant->value_i() == exact_constant;
    }
  }
};

template <typename Pattern>
class TypeCheckingPattern {
  Type* type;
  Pattern sub_pattern;

 public:
  explicit TypeCheckingPattern(Type* type, Pattern sub_pattern)
      : type(type), sub_pattern(sub_pattern) {}

  template <typename T>
  bool match(T* m_value) {
    if (m_value->type() == type) {
      return sub_pattern.match(m_value);
    }

    return false;
  }
};

template <typename Class, typename Pattern1, typename Pattern2>
class EitherPattern {
  Class** bind_value;
  Pattern1 pattern1;
  Pattern2 pattern2;

 public:
  explicit EitherPattern(Class** bind_value, Pattern1 pattern1, Pattern2 pattern2)
      : bind_value(bind_value), pattern1(pattern1), pattern2(pattern2) {}

  template <typename T>
  bool match(T* m_value) {
    const auto value = flugzeug::cast<std::remove_cv_t<Class>>(m_value);
    if (!value) {
      return false;
    }

    const bool matched = pattern1.match(value) || pattern2.match(value);
    if (!matched) {
      return false;
    }

    if (bind_value) {
      *bind_value = value;
    }

    return true;
  }
};

template <typename Class, typename Pattern1, typename Pattern2>
class BothPattern {
  Class** bind_value;
  Pattern1 pattern1;
  Pattern2 pattern2;

 public:
  explicit BothPattern(Class** bind_value, Pattern1 pattern1, Pattern2 pattern2)
      : bind_value(bind_value), pattern1(pattern1), pattern2(pattern2) {}

  template <typename T>
  bool match(T* m_value) {
    const auto value = flugzeug::cast<std::remove_cv_t<Class>>(m_value);
    if (!value) {
      return false;
    }

    const bool matched = pattern1.match(value) && pattern2.match(value);
    if (!matched) {
      return false;
    }

    if (bind_value) {
      *bind_value = value;
    }

    return true;
  }
};

}  // namespace detail

template <typename T = Value>
auto value() {
  return detail::ValueBindingPattern<const T>(nullptr);
}
template <typename T>
auto value(T*& v) {
  return detail::ValueBindingPattern<T>(&v);
}
template <typename T>
auto value(const T*& v) {
  return detail::ValueBindingPattern<const T>(&v);
}

inline auto constant() {
  return detail::ValueBindingPattern<const Constant>(nullptr);
}
inline auto constant(Constant*& v) {
  return detail::ValueBindingPattern<Constant>(&v);
}
inline auto constant(const Constant*& v) {
  return detail::ValueBindingPattern<const Constant>(&v);
}

inline auto undef() {
  return detail::ValueBindingPattern<const Undef>(nullptr);
}
inline auto undef(Undef*& v) {
  return detail::ValueBindingPattern<Undef>(&v);
}
inline auto undef(const Undef*& v) {
  return detail::ValueBindingPattern<const Undef>(&v);
}

inline auto block() {
  return detail::ValueBindingPattern<const Block>(nullptr);
}
inline auto block(Block*& v) {
  return detail::ValueBindingPattern<Block>(&v);
}
inline auto block(const Block*& v) {
  return detail::ValueBindingPattern<const Block>(&v);
}

inline auto function() {
  return detail::ValueBindingPattern<const Function>(nullptr);
}
inline auto function(Function*& v) {
  return detail::ValueBindingPattern<Function>(&v);
}
inline auto function(const Function*& v) {
  return detail::ValueBindingPattern<const Function>(&v);
}

inline auto constant_u(uint64_t& constant) {
  return detail::ConstantBindingPattern<true>(constant);
}
inline auto constant_i(int64_t& constant) {
  return detail::ConstantBindingPattern<false>(constant);
}

template <typename T>
auto exact(const T* value) {
  return detail::ExactValuePattern<T>(value);
}
template <typename T>
auto exact_ref(T*& value) {
  return detail::ExactValueRefPattern<T>(value);
}
template <typename T>
auto exact_ref(const T*& value) {
  return detail::ExactValueRefPattern<const T>(value);
}

inline auto exact_u(uint64_t value) {
  return detail::ExactConstantPattern<true>(value);
}
inline auto exact_i(int64_t value) {
  return detail::ExactConstantPattern<false>(value);
}

inline auto zero() {
  return detail::ExactConstantPattern<true>(0);
}
inline auto one() {
  return detail::ExactConstantPattern<true>(1);
}
inline auto negative_one() {
  return detail::ExactConstantPattern<false>(-1);
}

template <typename Pattern>
auto typed(Type* type, Pattern pattern) {
  return detail::TypeCheckingPattern<Pattern>(type, pattern);
}

template <typename T, typename Pattern1, typename Pattern2>
auto either(T*& v, Pattern1 pattern1, Pattern2 pattern2) {
  return detail::EitherPattern<T, Pattern1, Pattern2>(&v, pattern1, pattern2);
}
template <typename Pattern1, typename Pattern2>
auto either(Pattern1 pattern1, Pattern2 pattern2) {
  return detail::EitherPattern<const Value, Pattern1, Pattern2>(nullptr, pattern1, pattern2);
}

template <typename T, typename Pattern1, typename Pattern2>
auto both(T*& v, Pattern1 pattern1, Pattern2 pattern2) {
  return detail::BothPattern<T, Pattern1, Pattern2>(&v, pattern1, pattern2);
}
template <typename Pattern1, typename Pattern2>
auto both(Pattern1 pattern1, Pattern2 pattern2) {
  return detail::BothPattern<const Value, Pattern1, Pattern2>(nullptr, pattern1, pattern2);
}

}  // namespace flugzeug::pat