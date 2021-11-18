#pragma once
#include "Error.hpp"
#include <string_view>

#define BEGIN_ENUM_STRINGIFY(function_name, enum_type)                                             \
  static std::string_view function_name(enum_type e) {                                             \
    using ThisEnum = enum_type;                                                                    \
    switch (e) {

#define ENUM_CASE(enum_case)                                                                       \
  case ThisEnum::enum_case:                                                                        \
    return #enum_case;

#define END_ENUM_STRINGIFY()                                                                       \
  default:                                                                                         \
    unreachable();                                                                                 \
    }                                                                                              \
    }