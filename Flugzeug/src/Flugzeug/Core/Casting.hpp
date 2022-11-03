#pragma once
#include <type_traits>

#define DEFINE_INSTANCEOF(Base, kind)                \
 public:                                             \
  static bool is_object_of_class(const Base* base) { \
    return base->get_kind() == kind;                 \
  }                                                  \
                                                     \
 private:

#define DEFINE_INSTANCEOF_RANGE(Base, start, end)    \
 public:                                             \
  static bool is_object_of_class(const Base* base) { \
    const auto kind = base->get_kind();              \
    return kind >= start && kind <= end;             \
  }                                                  \
                                                     \
 private:

namespace flugzeug {

namespace detail {
template <typename To, typename From>
using CastResult = std::conditional_t<std::is_const_v<From>, const To*, To*>;
}

template <typename To, typename From>
detail::CastResult<To, From> relaxed_cast(From* from) {
  if constexpr (std::is_base_of_v<To, From>) {
    // We are upcasting, it will always succeed.
    return from;
  } else if constexpr (!std::is_base_of_v<From, To>) {
    // Pointer types are unrelated, the cast will always fail.
    return nullptr;
  } else {
    if (!from) {
      return nullptr;
    }
    return To::is_object_of_class(from) ? static_cast<detail::CastResult<To, From>>(from) : nullptr;
  }
}

template <typename To, typename From>
detail::CastResult<To, From> cast(From* from) {
  static_assert(std::is_base_of_v<To, From> || std::is_base_of_v<From, To>,
                "Casting between unrelated pointer types - it will always fail");

  return relaxed_cast<To, From>(from);
}

template <typename To, typename From>
detail::CastResult<To, From> cast(From& from) {
  return cast<To, From>(&from);
}

}  // namespace flugzeug