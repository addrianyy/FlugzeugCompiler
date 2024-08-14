#include "BrainfuckBufferSplitting.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

static bool process_pointer(Instruction* pointer,
                            int64_t offset_from_stackalloc,
                            Call* zero_buffer_call,
                            std::vector<std::pair<Instruction*, int64_t>>& worklist) {
  for (Instruction& user : pointer->users<Instruction>()) {
    if (const auto call = cast<Call>(user)) {
      if (call != zero_buffer_call) {
        return false;
      }

      continue;
    }

    if (const auto offset = cast<Offset>(user)) {
      const auto constant_index = cast<Constant>(offset->get_index());
      if (!constant_index) {
        return false;
      }

      worklist.emplace_back(offset, offset_from_stackalloc + constant_index->get_constant_i());
      continue;
    }

    {
      Value* accessed_pointer = nullptr;

      if (const auto load = cast<Load>(user)) {
        accessed_pointer = load->get_ptr();
      }

      if (const auto store = cast<Store>(user)) {
        accessed_pointer = store->get_ptr();
      }

      if (accessed_pointer != pointer) {
        return false;
      }
    }
  }

  return true;
}

static bool split_stackalloc(StackAlloc* stackalloc) {
  const auto type = stackalloc->get_allocated_type();

  auto zero_buffer_call = cast<Call>(stackalloc->next());
  if (zero_buffer_call && (zero_buffer_call->get_callee()->get_name() != "zero_buffer" ||
                           zero_buffer_call->get_arg(0) != stackalloc)) {
    zero_buffer_call = nullptr;
  }

  std::unordered_map<int64_t, std::vector<Instruction*>> offset_to_pointers;
  std::vector<std::pair<Instruction*, int64_t>> worklist;
  std::unordered_set<Value*> visited;

  worklist.emplace_back(stackalloc, 0);

  while (!worklist.empty()) {
    const auto [pointer, offset_from_stackalloc] = worklist.back();
    worklist.pop_back();

    if (!visited.insert(pointer).second) {
      continue;
    }

    if (!process_pointer(pointer, offset_from_stackalloc, zero_buffer_call, worklist)) {
      return false;
    }

    offset_to_pointers[offset_from_stackalloc].push_back(pointer);
  }

  for (const auto& [offset, pointers] : offset_to_pointers) {
    const auto partial_stackalloc = new StackAlloc(stackalloc->get_context(), type);
    partial_stackalloc->insert_after(stackalloc);

    const auto zero_initializer =
      new Store(stackalloc->get_context(), partial_stackalloc, type->get_zero());
    zero_initializer->insert_after(partial_stackalloc);

    for (const auto& pointer : pointers) {
      pointer->replace_uses_with(partial_stackalloc);
      if (pointer != stackalloc) {
        pointer->destroy();
      }
    }
  }

  if (zero_buffer_call) {
    zero_buffer_call->destroy();
  }
  stackalloc->destroy();

  return true;
}

bool bf::BrainfuckBufferSplitting::run(Function* function) {
  std::vector<StackAlloc*> stackallocs;

  for (StackAlloc& stackalloc : function->instructions<StackAlloc>()) {
    if (stackalloc.get_size() > 1) {
      stackallocs.push_back(&stackalloc);
    }
  }

  bool did_something = false;

  for (StackAlloc* stackalloc : stackallocs) {
    did_something |= split_stackalloc(stackalloc);
  }

  return did_something;
}