#include "PassRunner.hpp"

#include <Flugzeug/Core/Log.hpp>
#include <Flugzeug/IR/Function.hpp>

using namespace flugzeug;

OptimizationStatistics::StatisticsContext OptimizationStatistics::pre_pass_callback(
  std::string_view pass_name) {
  return {std::chrono::high_resolution_clock::now()};
}

void OptimizationStatistics::post_pass_callback(const StatisticsContext& context,
                                                std::string_view pass_name,
                                                bool success) {
  const auto end_time = std::chrono::high_resolution_clock::now();
  const auto start_time = context.start_time;
  const float elapsed = std::chrono::duration<float>(end_time - start_time).count();

  auto& pass_info = passes_info[pass_name];

  pass_info.invocations += 1;
  total_invocations += 1;

  if (success) {
    pass_info.successes += 1;
    total_successes += 1;
  }

  pass_info.time_spent += elapsed;
  total_time_spend += elapsed;
}

void OptimizationStatistics::show() const {
  constexpr auto sort_by_time = true;
  constexpr auto indentation = "    ";

  if (total_invocations == 0) {
    log_warn("No optimization statistics to show");
    return;
  }

  log_info("");
  {
    const auto total_success_ratio =
      int((float(total_successes) / float(total_invocations)) * 100.f);

    log_info("Optimization statistics:");
    log_info("{}Total invocations:   {}", indentation, total_invocations);
    log_info("{}Total successes:     {}", indentation, total_successes);
    log_info("{}Total success ratio: {}%", indentation, total_success_ratio);
    log_info("{}Total time spent:    {:.4f}s", indentation, total_time_spend);
  }

  log_info("");
  {
    std::vector<std::pair<std::string_view, const PassInfo*>> sorted_pass_info;
    sorted_pass_info.reserve(passes_info.size());

    for (const auto& [name, info] : passes_info) {
      sorted_pass_info.emplace_back(name, &info);
    }

    std::sort(sorted_pass_info.begin(), sorted_pass_info.end(),
              [&](const auto& a, const auto& b) -> bool {
                if (sort_by_time) {
                  return a.second->time_spent > b.second->time_spent;
                } else {
                  return a.second->successes > b.second->successes;
                }
              });

    log_info("Passes sorted by the {}:", sort_by_time ? "time" : "successes");

    for (size_t i = 0; i < sorted_pass_info.size(); ++i) {
      const auto& [pass_name, info] = sorted_pass_info[i];
      const auto success_ratio = int((float(info->successes) / float(info->invocations)) * 100.f);
      const auto time_spent_ratio = int((info->time_spent / total_time_spend) * 100.f);

      log_info(
        "{}{:>2}. {:<35} | {:>3} invocations | {:>3} successes | {:>3}% success ratio | "
        "{:>7.3f}s time spent | {:>3}% time spent",
        indentation, i + 1, pass_name, info->invocations, info->successes, success_ratio,
        info->time_spent, time_spent_ratio);
    }
  }

  log_info("");
}

void OptimizationStatistics::clear() {
  total_invocations = 0;
  total_successes = 0;
  total_time_spend = 0.f;

  passes_info.clear();
}

void FunctionPassRunner::on_finished_optimization(Function* function) {
  function->reassign_display_indices();
}

void FunctionPassRunner::validate() const {
  function->validate(ValidationBehaviour::ErrorsAreFatal);
}