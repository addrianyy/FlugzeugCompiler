#pragma once
#include <Flugzeug/IR/Instructions.hpp>

#include "General.hpp"

namespace flugzeug::pat {

namespace detail {

template <typename PtrPattern> class LoadPattern {
  Load** bind_instruction;
  PtrPattern ptr;

public:
  LoadPattern(Load** bind_instruction, PtrPattern ptr)
      : bind_instruction(bind_instruction), ptr(ptr) {}

  template <typename T> bool match(T* m_value) {
    const auto load = ::cast<Load>(m_value);
    if (!load) {
      return false;
    }

    const bool matched = ptr.match(load->get_ptr());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = load;
    }

    return true;
  }
};

template <typename PtrPattern, typename ValuePattern> class StorePattern {
  Store** bind_instruction;
  PtrPattern ptr;
  ValuePattern value;

public:
  StorePattern(Store** bind_instruction, PtrPattern ptr, ValuePattern value)
      : bind_instruction(bind_instruction), ptr(ptr), value(value) {}

  template <typename T> bool match(T* m_value) {
    const auto store = ::cast<Store>(m_value);
    if (!store) {
      return false;
    }

    const bool matched = ptr.match(store->get_ptr()) && value.match(store->get_val());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = store;
    }

    return true;
  }
};

template <typename CalleePattern> class CallPattern {
  Call** bind_instruction;
  CalleePattern callee;

public:
  CallPattern(Call** bind_instruction, CalleePattern callee)
      : bind_instruction(bind_instruction), callee(callee) {}

  template <typename T> bool match(T* m_value) {
    const auto call = ::cast<Call>(m_value);
    if (!call) {
      return false;
    }

    const bool matched = callee.match(call->get_callee());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = call;
    }

    return true;
  }
};

template <typename TargetPattern> class BranchPattern {
  Branch** bind_instruction;
  TargetPattern target;

public:
  BranchPattern(Branch** bind_instruction, TargetPattern target)
      : bind_instruction(bind_instruction), target(target) {}

  template <typename T> bool match(T* m_value) {
    const auto branch = ::cast<Branch>(m_value);
    if (!branch) {
      return false;
    }

    const bool matched = target.match(branch->get_target());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = branch;
    }

    return true;
  }
};

template <typename TInstruction, typename CondPattern, typename TruePattern, typename FalsePattern>
class CondBranchOrSelectPattern {
  TInstruction** bind_instruction;
  CondPattern cond;
  TruePattern on_true;
  FalsePattern on_false;

public:
  CondBranchOrSelectPattern(TInstruction** bind_instruction, CondPattern cond, TruePattern on_true,
                            FalsePattern on_false)
      : bind_instruction(bind_instruction), cond(cond), on_true(on_true), on_false(on_false) {}

  template <typename T> bool match(T* m_value) {
    const auto instruction = ::cast<TInstruction>(m_value);
    if (!instruction) {
      return false;
    }

    bool matched;
    if constexpr (std::is_same_v<Instruction, CondBranch>) {
      matched = cond.match(instruction->get_cond()) &&
                on_true.match(instruction->get_true_target()) &&
                on_false.match(instruction->get_false_target());
    } else {
      matched = cond.match(instruction->get_cond()) && on_true.match(instruction->get_true_val()) &&
                on_false.match(instruction->get_false_val());
    }

    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = instruction;
    }

    return true;
  }
};

class RetVoidPattern {
  Ret** bind_instruction;

public:
  explicit RetVoidPattern(Ret** bind_instruction) : bind_instruction(bind_instruction) {}

  template <typename T> bool match(T* m_value) {
    const auto ret = ::cast<Ret>(m_value);
    if (!ret || ret->get_val()) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = ret;
    }

    return true;
  }
};

template <typename ValuePattern> class RetPattern {
  Ret** bind_instruction;
  ValuePattern value;

public:
  RetPattern(Ret** bind_instruction, ValuePattern value)
      : bind_instruction(bind_instruction), value(value) {}

  template <typename T> bool match(T* m_value) {
    const auto ret = ::cast<Ret>(m_value);
    if (!ret || !ret->get_val()) {
      return false;
    }

    const bool matched = value.match(ret->get_val());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = ret;
    }

    return true;
  }
};

template <typename BasePattern, typename IndexPattern> class OffsetPattern {
  Offset** bind_instruction;
  BasePattern base;
  IndexPattern index;

public:
  OffsetPattern(Offset** bind_instruction, BasePattern base, IndexPattern index)
      : bind_instruction(bind_instruction), base(base), index(index) {}

  template <typename T> bool match(T* m_value) {
    const auto offset = ::cast<Offset>(m_value);
    if (!offset) {
      return false;
    }

    const bool matched = base.match(offset->get_base()) && index.match(offset->get_index());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = offset;
    }

    return true;
  }
};

} // namespace detail

template <typename PtrPattern> auto load(Load*& instruction, PtrPattern ptr) {
  return detail::LoadPattern<PtrPattern>(&instruction, ptr);
}

template <typename PtrPattern> auto load(PtrPattern ptr) {
  return detail::LoadPattern<PtrPattern>(nullptr, ptr);
}

template <typename PtrPattern, typename ValuePattern>
auto store(Store*& instruction, PtrPattern ptr, ValuePattern value) {
  return detail::StorePattern<PtrPattern, ValuePattern>(&instruction, ptr, value);
}

template <typename PtrPattern, typename ValuePattern>
auto store(PtrPattern ptr, ValuePattern value) {
  return detail::StorePattern<PtrPattern, ValuePattern>(nullptr, ptr, value);
}

template <typename CalleePattern> auto call(Call*& instruction, CalleePattern callee) {
  return detail::CallPattern<CalleePattern>(&instruction, callee);
}

template <typename CalleePattern> auto call(CalleePattern callee) {
  return detail::CallPattern<CalleePattern>(nullptr, callee);
}

template <typename TargetPattern> auto branch(Branch*& instruction, TargetPattern target) {
  return detail::BranchPattern<TargetPattern>(&instruction, target);
}

template <typename TargetPattern> auto branch(TargetPattern target) {
  return detail::BranchPattern<TargetPattern>(nullptr, target);
}

template <typename CondPattern, typename TruePattern, typename FalsePattern>
auto cond_branch(CondBranch*& instruction, CondPattern cond, TruePattern on_true,
                 FalsePattern on_false) {
  return detail::CondBranchOrSelectPattern<CondBranch, CondPattern, TruePattern, FalsePattern>(
    &instruction, cond, on_true, on_false);
}

template <typename CondPattern, typename TruePattern, typename FalsePattern>
auto cond_branch(CondPattern cond, TruePattern on_true, FalsePattern on_false) {
  return detail::CondBranchOrSelectPattern<CondBranch, CondPattern, TruePattern, FalsePattern>(
    nullptr, cond, on_true, on_false);
}

inline auto stackalloc(StackAlloc*& instruction) {
  return detail::ClassFilteringPattern<StackAlloc>(&instruction);
}
inline auto stackalloc() { return detail::ClassFilteringPattern<StackAlloc>(nullptr); }

inline auto ret_void(Ret*& instruction) { return detail::RetVoidPattern(&instruction); }
inline auto ret_void() { return detail::RetVoidPattern(nullptr); }

template <typename ValuePattern> auto ret(Ret*& instruction, ValuePattern value) {
  return detail::RetPattern<ValuePattern>(&instruction, value);
}

template <typename ValuePattern> auto ret(ValuePattern value) {
  return detail::RetPattern<ValuePattern>(nullptr, value);
}

template <typename BasePattern, typename IndexPattern>
auto offset(Offset*& instruction, BasePattern base, IndexPattern index) {
  return detail::OffsetPattern<BasePattern, IndexPattern>(&instruction, base, index);
}

template <typename BasePattern, typename IndexPattern>
auto offset(BasePattern base, IndexPattern index) {
  return detail::OffsetPattern<BasePattern, IndexPattern>(nullptr, base, index);
}

template <typename CondPattern, typename TruePattern, typename FalsePattern>
auto select(Select*& instruction, CondPattern cond, TruePattern on_true, FalsePattern on_false) {
  return detail::CondBranchOrSelectPattern<Select, CondPattern, TruePattern, FalsePattern>(
    &instruction, cond, on_true, on_false);
}

template <typename CondPattern, typename TruePattern, typename FalsePattern>
auto select(CondPattern cond, TruePattern on_true, FalsePattern on_false) {
  return detail::CondBranchOrSelectPattern<Select, CondPattern, TruePattern, FalsePattern>(
    nullptr, cond, on_true, on_false);
}

inline auto phi(Phi*& instruction) { return detail::ClassFilteringPattern<Phi>(&instruction); }
inline auto phi() { return detail::ClassFilteringPattern<Phi>(nullptr); }

} // namespace flugzeug::pat