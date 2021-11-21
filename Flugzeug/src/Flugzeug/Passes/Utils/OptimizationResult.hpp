#pragma once
#include <Flugzeug/Core/Error.hpp>

namespace flugzeug {

class Value;

class OptimizationResult {
  Value* replacement = nullptr;
  bool successful = false;

  explicit OptimizationResult(bool successful) : successful(successful) {}

public:
  OptimizationResult(Value* replacement) : replacement(replacement), successful(true) {
    verify(replacement, "Cannot use null replacement for `OptimizationResult`");
  }

  static inline OptimizationResult replace_instructon(Value* replacement) { return {replacement}; }

  static inline OptimizationResult changed() { return OptimizationResult(true); }
  static inline OptimizationResult unchanged() { return OptimizationResult(false); }

  operator bool() const { return successful; }

  bool was_successful() const { return successful; }
  Value* get_replacement() const { return replacement; }
};

} // namespace flugzeug