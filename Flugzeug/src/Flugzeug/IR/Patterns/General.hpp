#pragma once
#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug::pat {

namespace detail {

template <typename Class> class ClassFilteringPattern {
  Class** bind_value;

public:
  explicit ClassFilteringPattern(Class** bind_value) : bind_value(bind_value) {}

  template <typename T> bool match(T* m_value) {
    const auto value = ::cast<Class>(m_value);
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
    const auto constant = ::cast<Constant>(m_value);
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

template <typename Pattern> class TypedPattern {
  Type* type;
  Pattern sub_pattern;

public:
  explicit TypedPattern(Type* type, Pattern sub_pattern) : type(type), sub_pattern(sub_pattern) {}

  template <typename T> bool match(T* m_value) {
    if (m_value->get_type() == type) {
      return sub_pattern.match(m_value);
    }

    return false;
  }
};

template <typename TValue> class SpecificValuePattern {
  const TValue* specific_value;

public:
  explicit SpecificValuePattern(const TValue* specific_value) : specific_value(specific_value) {}

  template <typename T> bool match(T* m_value) {
    const auto value = ::cast<TValue>(m_value);
    if (!value) {
      return false;
    }

    return value == specific_value;
  }
};

template <bool Unsigned> class SpecificConstantPattern {
public:
  using Type = std::conditional_t<Unsigned, uint64_t, int64_t>;

private:
  Type specific_value;

public:
  explicit SpecificConstantPattern(Type specific_value) : specific_value(specific_value) {}

  template <typename T> bool match(T* m_value) {
    const auto constant = ::cast<Constant>(m_value);
    if (!constant) {
      return false;
    }

    if constexpr (Unsigned) {
      return constant->get_constant_u() == specific_value;
    } else {
      return constant->get_constant_i() == specific_value;
    }
  }
};

} // namespace detail

template <typename T = Value> auto value() { return detail::ClassFilteringPattern<Value>(nullptr); }
template <typename T = Value> auto value(T*& v) { return detail::ClassFilteringPattern<T>(&v); }
template <typename T = Value> auto value(const T*& v) {
  return detail::ClassFilteringPattern<const T>(&v);
}

inline auto constant() { return detail::ClassFilteringPattern<const Constant>(nullptr); }
inline auto constant(Constant*& v) { return detail::ClassFilteringPattern<Constant>(&v); }
inline auto constant(const Constant*& v) {
  return detail::ClassFilteringPattern<const Constant>(&v);
}

inline auto undef() { return detail::ClassFilteringPattern<Undef>(nullptr); }
inline auto undef(Undef*& v) { return detail::ClassFilteringPattern<Undef>(&v); }
inline auto undef(const Undef*& v) { return detail::ClassFilteringPattern<const Undef>(&v); }

inline auto block() { return detail::ClassFilteringPattern<Block>(nullptr); }
inline auto block(Block*& v) { return detail::ClassFilteringPattern<Block>(&v); }
inline auto block(const Block*& v) { return detail::ClassFilteringPattern<const Block>(&v); }

inline auto function() { return detail::ClassFilteringPattern<Function>(nullptr); }
inline auto function(Function*& v) { return detail::ClassFilteringPattern<Function>(&v); }
inline auto function(const Function*& v) {
  return detail::ClassFilteringPattern<const Function>(&v);
}

inline auto constant_u(uint64_t& constant) {
  return detail::ConstantExtractionPattern<true>(constant);
}
inline auto constant_i(int64_t& constant) {
  return detail::ConstantExtractionPattern<false>(constant);
}

template <typename T> auto specific_value(const T* value) {
  return detail::SpecificValuePattern<T>(value);
}

inline auto specific_block(const Block* block) {
  return detail::SpecificValuePattern<Block>(block);
}

inline auto specific_function(const Function* function) {
  return detail::SpecificValuePattern<Function>(function);
}

inline auto specific_constant(uint64_t value) {
  return detail::SpecificConstantPattern<true>(value);
}
inline auto specific_constant(int64_t value) {
  return detail::SpecificConstantPattern<false>(value);
}

inline auto zero() { return detail::SpecificConstantPattern<true>(0); }
inline auto one() { return detail::SpecificConstantPattern<true>(1); }
inline auto negative_one() { return detail::SpecificConstantPattern<false>(-1); }

template <typename Pattern> auto typed(Type* type, Pattern pattern) {
  return detail::TypedPattern<Pattern>(type, pattern);
}

} // namespace flugzeug::pat