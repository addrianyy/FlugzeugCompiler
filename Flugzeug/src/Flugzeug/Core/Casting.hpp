#pragma once
#include <type_traits>

#define DEFINE_INSTANCEOF(Base, kind)                                                              \
public:                                                                                            \
  static bool is_object_of_class(const Base* base) { return base->get_kind() == kind; }            \
                                                                                                   \
private:

#define DEFINE_INSTANCEOF_RANGE(Base, start, end)                                                  \
public:                                                                                            \
  static bool is_object_of_class(const Base* base) {                                               \
    const auto kind = base->get_kind();                                                            \
    return kind >= start && kind <= end;                                                           \
  }                                                                                                \
                                                                                                   \
private:

namespace flugzeug {

template <typename To, typename From> To* cast(From* from) {
  if constexpr (std::is_base_of_v<To, From>) {
    // We are upcasting, it will always succeed.
    return from;
  } else if constexpr (!std::is_base_of_v<From, To>) {
    // Pointer types are unrelated, the cast will always fail.
    // TODO: Maybe add a warning here (?)
    return nullptr;
  } else {
    if (!from) {
      return nullptr;
    }
    return To::is_object_of_class(from) ? static_cast<To*>(from) : nullptr;
  }
}

template <typename To, typename From> const To* cast(const From* from) {
  if constexpr (std::is_base_of_v<To, From>) {
    // We are upcasting, it will always succeed.
    return from;
  } else if constexpr (!std::is_base_of_v<From, To>) {
    // Pointer types are unrelated, the cast will always fail.
    // TODO: Maybe add a warning here (?)
    return nullptr;
  } else {
    if (!from) {
      return nullptr;
    }
    return To::is_object_of_class(from) ? static_cast<const To*>(from) : nullptr;
  }
}

template <typename To, typename From> To* cast(From& from) { return cast<To>(&from); }
template <typename To, typename From> const To* cast(const From& from) { return cast<To>(&from); }

} // namespace flugzeug