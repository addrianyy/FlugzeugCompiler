#include "CallInlining.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/Passes/Utils/Inline.hpp>

using namespace flugzeug;

static bool inline_everything(Function* function) {
  std::vector<Call*> inlinable_calls;

  for (Call& call : function->instructions<Call>()) {
    Function* callee = call.get_callee();
    if (!callee->is_extern() && callee != function) {
      inlinable_calls.push_back(&call);
    }
  }

  for (Call* call : inlinable_calls) {
    flugzeug::utils::inline_call(call);
  }

  return !inlinable_calls.empty();
}

bool CallInlining::run(Function* function, InliningStrategy strategy) {
  switch (strategy) {
  case InliningStrategy::InlineEverything:
    return inline_everything(function);

  default:
    unreachable();
  }
}