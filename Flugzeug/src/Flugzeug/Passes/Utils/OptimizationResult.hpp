#pragma once
#include <Flugzeug/Core/Error.hpp>
#include <Flugzeug/IR/InstructionInserter.hpp>

namespace flugzeug {

class Value;

using Rewriter = InstructionInserter;

class OptimizationResult {
  Value* replacement = nullptr;
  bool successful = false;

  explicit OptimizationResult(bool successful) : successful(successful) {}

 public:
  OptimizationResult(Value* replacement) : replacement(replacement), successful(true) {
    verify(replacement, "Cannot use null replacement for `OptimizationResult`");
  }

  template <typename Fn>
  static OptimizationResult rewrite(Instruction* instruction, Fn&& fn) {
    InstructionInserter inserter(instruction, InsertDestination::Back, true);
    Value* result = fn(inserter);
    verify(result, "Failed to rewrite instruction");
    return result;
  }

  static inline OptimizationResult replace_instructon(Value* replacement) { return {replacement}; }

  static inline OptimizationResult changed() { return OptimizationResult(true); }
  static inline OptimizationResult unchanged() { return OptimizationResult(false); }

  operator bool() const { return successful; }

  bool was_successful() const { return successful; }
  Value* get_replacement() const { return replacement; }
};

}  // namespace flugzeug