#pragma once
#include <algorithm>
#include <string_view>

namespace flugzeug {

class Function;

namespace detail {
class PassBase {};

template <size_t N>
struct StringLiteral {
  constexpr StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }

  char value[N];
};
}  // namespace detail

template <detail::StringLiteral PassName>
class Pass : public detail::PassBase {
 public:
  consteval static std::string_view pass_name() { return std::string_view(PassName.value); }
};

}  // namespace flugzeug