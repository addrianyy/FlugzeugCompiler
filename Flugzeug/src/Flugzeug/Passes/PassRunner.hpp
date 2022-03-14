#pragma once
#include "Pass.hpp"

#include <Flugzeug/Core/ClassTraits.hpp>

#include <chrono>
#include <unordered_map>
#include <utility>

namespace flugzeug {

class Function;

class OptimizationStatistics {
  friend class FunctionPassRunner;

  struct StatisticsContext {
    std::chrono::high_resolution_clock::time_point start_time;
  };

  struct PassInfo {
    size_t invocations = 0;
    size_t successes = 0;
    float time_spent = 0.f;
  };

  std::unordered_map<std::string_view, PassInfo> passes_info;

  size_t total_invocations = 0;
  size_t total_successes = 0;
  float total_time_spend = 0.f;

  StatisticsContext pre_pass_callback(std::string_view pass_name);
  void post_pass_callback(const StatisticsContext& context, std::string_view pass_name,
                          bool success);

public:
  OptimizationStatistics() = default;

  void show() const;
  void clear();
};

class FunctionPassRunner {
  Function* function = nullptr;
  OptimizationStatistics* statistics = nullptr;
  bool strict_validation = false;

  bool did_something_ = false;

  static void on_finished_optimization(Function* function);

  void validate() const;

public:
  CLASS_NON_COPYABLE(FunctionPassRunner)

  explicit FunctionPassRunner(Function* function, bool strict_validation = false)
      : function(function), strict_validation(strict_validation) {}

  explicit FunctionPassRunner(Function* function, OptimizationStatistics* statistics,
                              bool strict_validation = false)
      : function(function), statistics(statistics), strict_validation(strict_validation) {}

  template <typename T, typename... Args> bool run(Args&&... args) {
    static_assert(std::is_base_of_v<detail::PassBase, T>,
                  "Optimization pass must inherit from Pass class.");

    const auto pass_name = T::pass_name();

    OptimizationStatistics::StatisticsContext statistics_context{};

    if (statistics) {
      statistics_context = statistics->pre_pass_callback(pass_name);
    }

    const bool success = T::run(function, std::forward<Args>(args)...);

    if (statistics) {
      statistics->post_pass_callback(statistics_context, pass_name, success);
    }

    did_something_ |= success;

    if (strict_validation) {
      validate();
    }

    return did_something_;
  }

  bool did_something() const { return did_something_; }

  template <typename OptCallback>
  static bool enter_optimization_loop(Function* function, OptimizationStatistics* statistics,
                                      bool strict_validation, OptCallback opt_callback) {
    bool did_something = false;

    while (true) {
      FunctionPassRunner runner(function, statistics, strict_validation);
      opt_callback(runner);

      did_something |= runner.did_something();
      if (!runner.did_something()) {
        on_finished_optimization(function);
        break;
      }
    }

    return did_something;
  }

  template <typename OptCallback>
  static bool enter_optimization_loop(Function* function, OptimizationStatistics* statistics,
                                      OptCallback opt_callback) {
    return enter_optimization_loop(function, statistics, false, opt_callback);
  }

  template <typename OptCallback>
  static bool enter_optimization_loop(Function* function, bool strict_validation,
                                      OptCallback opt_callback) {
    return enter_optimization_loop(function, nullptr, strict_validation, opt_callback);
  }

  template <typename OptCallback>
  static bool enter_optimization_loop(Function* function, OptCallback opt_callback) {
    return enter_optimization_loop(function, nullptr, false, opt_callback);
  }
};

} // namespace flugzeug