#pragma once
#include <Flugzeug/IR/Instructions.hpp>

#include "General.hpp"

namespace flugzeug::pat {

namespace detail {

template <typename TInstruction, typename PtrPattern> class LoadPattern {
  TInstruction** bind_instruction;
  PtrPattern ptr;

public:
  LoadPattern(TInstruction** bind_instruction, PtrPattern ptr)
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

template <typename TInstruction, typename PtrPattern, typename ValuePattern> class StorePattern {
  TInstruction** bind_instruction;
  PtrPattern ptr;
  ValuePattern value;

public:
  StorePattern(TInstruction** bind_instruction, PtrPattern ptr, ValuePattern value)
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

template <typename TInstruction, typename CalleePattern> class CallPattern {
  TInstruction** bind_instruction;
  CalleePattern callee;

public:
  CallPattern(TInstruction** bind_instruction, CalleePattern callee)
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

template <typename TInstruction, typename TargetPattern> class BranchPattern {
  TInstruction** bind_instruction;
  TargetPattern target;

public:
  BranchPattern(TInstruction** bind_instruction, TargetPattern target)
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

template <typename TInstruction> class RetVoidPattern {
  TInstruction** bind_instruction;

public:
  explicit RetVoidPattern(TInstruction** bind_instruction) : bind_instruction(bind_instruction) {}

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

template <typename TInstruction, typename ValuePattern> class RetPattern {
  TInstruction** bind_instruction;
  ValuePattern value;

public:
  RetPattern(TInstruction** bind_instruction, ValuePattern value)
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

template <typename TInstruction, typename BasePattern, typename IndexPattern> class OffsetPattern {
  TInstruction** bind_instruction;
  BasePattern base;
  IndexPattern index;

public:
  OffsetPattern(TInstruction** bind_instruction, BasePattern base, IndexPattern index)
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

template <typename TInstruction, typename PtrPattern>
auto load(TInstruction*& instruction, PtrPattern ptr) {
  static_assert(std::is_same_v<Load, std::remove_cv_t<TInstruction>>,
                "Expected Load instruction in this pattern");
  return detail::LoadPattern<TInstruction, PtrPattern>(&instruction, ptr);
}

template <typename PtrPattern> auto load(PtrPattern ptr) {
  return detail::LoadPattern<const Load, PtrPattern>(nullptr, ptr);
}

template <typename TInstruction, typename PtrPattern, typename ValuePattern>
auto store(TInstruction*& instruction, PtrPattern ptr, ValuePattern value) {
  static_assert(std::is_same_v<Store, std::remove_cv_t<TInstruction>>,
                "Expected Store instruction in this pattern");
  return detail::StorePattern<TInstruction, PtrPattern, ValuePattern>(&instruction, ptr, value);
}

template <typename PtrPattern, typename ValuePattern>
auto store(PtrPattern ptr, ValuePattern value) {
  return detail::StorePattern<const Store, PtrPattern, ValuePattern>(nullptr, ptr, value);
}

template <typename TInstruction, typename CalleePattern>
auto call(TInstruction*& instruction, CalleePattern callee) {
  static_assert(std::is_same_v<Call, std::remove_cv_t<TInstruction>>,
                "Expected Call instruction in this pattern");
  return detail::CallPattern<TInstruction, CalleePattern>(&instruction, callee);
}

template <typename CalleePattern> auto call(CalleePattern callee) {
  return detail::CallPattern<const Call, CalleePattern>(nullptr, callee);
}

template <typename TInstruction, typename TargetPattern>
auto branch(TInstruction*& instruction, TargetPattern target) {
  static_assert(std::is_same_v<Branch, std::remove_cv_t<TInstruction>>,
                "Expected Branch instruction in this pattern");
  return detail::BranchPattern<TInstruction, TargetPattern>(&instruction, target);
}

template <typename TargetPattern> auto branch(TargetPattern target) {
  return detail::BranchPattern<const Branch, TargetPattern>(nullptr, target);
}

template <typename TInstruction, typename CondPattern, typename TruePattern, typename FalsePattern>
auto cond_branch(TInstruction*& instruction, CondPattern cond, TruePattern on_true,
                 FalsePattern on_false) {
  static_assert(std::is_same_v<CondBranch, std::remove_cv_t<TInstruction>>,
                "Expected CondBranch instruction in this pattern");
  return detail::CondBranchOrSelectPattern<TInstruction, CondPattern, TruePattern, FalsePattern>(
    &instruction, cond, on_true, on_false);
}

template <typename CondPattern, typename TruePattern, typename FalsePattern>
auto cond_branch(CondPattern cond, TruePattern on_true, FalsePattern on_false) {
  return detail::CondBranchOrSelectPattern<const CondBranch, CondPattern, TruePattern,
                                           FalsePattern>(nullptr, cond, on_true, on_false);
}

template <typename TInstruction> auto stackalloc(TInstruction*& instruction) {
  static_assert(std::is_same_v<StackAlloc, std::remove_cv_t<TInstruction>>,
                "Expected StackAlloc instruction in this pattern");
  return detail::ClassFilteringPattern<TInstruction>(&instruction);
}
inline auto stackalloc() { return detail::ClassFilteringPattern<const StackAlloc>(nullptr); }

template <typename TInstruction> auto ret_void(TInstruction*& instruction) {
  static_assert(std::is_same_v<Ret, std::remove_cv_t<TInstruction>>,
                "Expected Ret instruction in this pattern");
  return detail::RetVoidPattern<TInstruction>(&instruction);
}
inline auto ret_void() { return detail::RetVoidPattern<const Ret>(nullptr); }

template <typename TInstruction, typename ValuePattern>
auto ret(TInstruction*& instruction, ValuePattern value) {
  static_assert(std::is_same_v<Ret, std::remove_cv_t<TInstruction>>,
                "Expected Ret instruction in this pattern");
  return detail::RetPattern<TInstruction, ValuePattern>(&instruction, value);
}

template <typename ValuePattern> auto ret(ValuePattern value) {
  return detail::RetPattern<const Ret, ValuePattern>(nullptr, value);
}

template <typename TInstruction, typename BasePattern, typename IndexPattern>
auto offset(TInstruction*& instruction, BasePattern base, IndexPattern index) {
  static_assert(std::is_same_v<Offset, std::remove_cv_t<TInstruction>>,
                "Expected Offset instruction in this pattern");
  return detail::OffsetPattern<TInstruction, BasePattern, IndexPattern>(&instruction, base, index);
}

template <typename BasePattern, typename IndexPattern>
auto offset(BasePattern base, IndexPattern index) {
  return detail::OffsetPattern<const Offset, BasePattern, IndexPattern>(nullptr, base, index);
}

template <typename TInstruction, typename CondPattern, typename TruePattern, typename FalsePattern>
auto select(TInstruction*& instruction, CondPattern cond, TruePattern on_true,
            FalsePattern on_false) {
  static_assert(std::is_same_v<Select, std::remove_cv_t<TInstruction>>,
                "Expected Select instruction in this pattern");
  return detail::CondBranchOrSelectPattern<TInstruction, CondPattern, TruePattern, FalsePattern>(
    &instruction, cond, on_true, on_false);
}

template <typename CondPattern, typename TruePattern, typename FalsePattern>
auto select(CondPattern cond, TruePattern on_true, FalsePattern on_false) {
  return detail::CondBranchOrSelectPattern<const Select, CondPattern, TruePattern, FalsePattern>(
    nullptr, cond, on_true, on_false);
}

template <typename TInstruction> auto phi(TInstruction*& instruction) {
  static_assert(std::is_same_v<Phi, std::remove_cv_t<TInstruction>>,
                "Expected Phi instruction in this pattern");
  return detail::ClassFilteringPattern<TInstruction>(&instruction);
}
inline auto phi() { return detail::ClassFilteringPattern<const Phi>(nullptr); }

} // namespace flugzeug::pat