#pragma once
#include <utility>

namespace flugzeug {

class Function;

class FunctionPassRunner {
  Function* function = nullptr;
  bool strict_validation = false;
  bool did_something_ = false;

  void validate() const;

public:
  explicit FunctionPassRunner(Function* function, bool strict_validation = false)
      : function(function), strict_validation(strict_validation) {}

  template <typename T, typename... Args> bool run(Args&&... args) {
    const bool success = T::run(function, std::forward<Args>(args)...);
    did_something_ |= success;

    if (strict_validation) {
      validate();
    }

    return did_something_;
  }

  bool did_something() const { return did_something_; }
};

} // namespace flugzeug