#pragma once
#include "Pass.hpp"
#include <Flugzeug/Core/ClassTraits.hpp>
#include <utility>

namespace flugzeug {

class Function;

class FunctionPassRunner {
  Function* function = nullptr;
  bool strict_validation = false;
  bool did_something_ = false;

  static void on_finished_optimization(Function* function);

  void validate() const;

public:
  CLASS_NON_COPYABLE(FunctionPassRunner)

  explicit FunctionPassRunner(Function* function, bool strict_validation = false)
      : function(function), strict_validation(strict_validation) {}

  template <typename T, typename... Args> bool run(Args&&... args) {
    static_assert(std::is_base_of_v<detail::PassBase, T>,
                  "Optimization pass must inherit from Pass class.");

    constexpr auto pass_name = T::pass_name();
    const bool success = T::run(function, std::forward<Args>(args)...);
    did_something_ |= success;

    if (strict_validation) {
      validate();
    }

    return did_something_;
  }

  bool did_something() const { return did_something_; }

  template <typename OptCallback>
  static bool enter_optimization_loop(Function* function, OptCallback opt_callback) {
    bool did_something = false;

    while (true) {
      FunctionPassRunner runner(function);
      opt_callback(runner);

      did_something |= runner.did_something();
      if (!runner.did_something()) {
        on_finished_optimization(function);
        break;
      }
    }

    return did_something;
  }
};

} // namespace flugzeug