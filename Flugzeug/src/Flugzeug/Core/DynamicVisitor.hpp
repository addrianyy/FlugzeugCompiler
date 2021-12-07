#pragma once
#include <type_traits>
#include <utility>

namespace dynamic_visitor_detail {

template <typename T> struct VisitedType { using ArgumentType = std::remove_cvref<T>; };

} // namespace dynamic_visitor_detail

template <typename T> bool dynamic_visitor(T&& object) { return false; }

template <typename T, typename Type, typename Callable, typename... Args>
void dynamic_visitor(T&& object, Type, Callable&& callable, Args&&... args) {
  if (auto casted = cast<typename Type::ArgumentType>(object)) {
    callable(casted);
  } else {
    dynamic_visitor(object, std::forward<Args>(args)...);
  }
}

#define visit_type(type, name) ::dynamic_visitor_detail::VisitedType<type>{}, [&](type * name)

/*

Example usage:
dynamic_visitor(
  instruction,
  visit_type(Store, store) { log_debug("store"); },
  visit_type(StackAlloc, stackalloc) { log_debug("stackalloc"); },
  visit_type(Instruction, instruction) { log_debug("instruction"); }
);

 */