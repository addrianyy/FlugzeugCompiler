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

template <typename To, typename From,
          typename std::enable_if_t<std::is_base_of_v<To, From>, int> = 0>
inline To* cast(From* from) {
  return (To*)from;
}
template <typename To, typename From,
          typename std::enable_if_t<std::is_base_of_v<To, From>, int> = 0>
inline const To* cast(const From* from) {
  return (const To*)from;
}

template <typename To, typename From,
          typename std::enable_if_t<!std::is_base_of_v<To, From>, int> = 0>
inline To* cast(From* from) {
  if (!from) {
    return nullptr;
  }
  return To::is_object_of_class(from) ? (To*)from : nullptr;
}
template <typename To, typename From,
          typename std::enable_if_t<!std::is_base_of_v<To, From>, int> = 0>
inline const To* cast(const From* from) {
  if (!from) {
    return nullptr;
  }
  return To::is_object_of_class(from) ? (const To*)from : nullptr;
}