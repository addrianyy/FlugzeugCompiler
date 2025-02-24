#pragma once
#include <Flugzeug/IR/Instructions.hpp>

#include "General.hpp"

namespace flugzeug::pat {

namespace detail {

template <typename TInstruction, typename AddressPattern>
class LoadPattern {
  TInstruction** bind_instruction;
  AddressPattern address;

 public:
  LoadPattern(TInstruction** bind_instruction, AddressPattern address)
      : bind_instruction(bind_instruction), address(address) {}

  template <typename T>
  bool match(T* m_value) {
    const auto load = flugzeug::cast<Load>(m_value);
    if (!load) {
      return false;
    }

    const bool matched = address.match(load->address());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = load;
    }

    return true;
  }
};

template <typename TInstruction, typename AddressPattern, typename ValuePattern>
class StorePattern {
  TInstruction** bind_instruction;
  AddressPattern address;
  ValuePattern value;

 public:
  StorePattern(TInstruction** bind_instruction, AddressPattern address, ValuePattern value)
      : bind_instruction(bind_instruction), address(address), value(value) {}

  template <typename T>
  bool match(T* m_value) {
    const auto store = flugzeug::cast<Store>(m_value);
    if (!store) {
      return false;
    }

    const bool matched = address.match(store->address()) && value.match(store->value());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = store;
    }

    return true;
  }
};

template <typename TInstruction, typename CalleePattern>
class CallPattern {
  TInstruction** bind_instruction;
  CalleePattern callee;

 public:
  CallPattern(TInstruction** bind_instruction, CalleePattern callee)
      : bind_instruction(bind_instruction), callee(callee) {}

  template <typename T>
  bool match(T* m_value) {
    const auto call = flugzeug::cast<Call>(m_value);
    if (!call) {
      return false;
    }

    const bool matched = callee.match(call->callee());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = call;
    }

    return true;
  }
};

template <typename TInstruction, typename TargetPattern>
class BranchPattern {
  TInstruction** bind_instruction;
  TargetPattern target;

 public:
  BranchPattern(TInstruction** bind_instruction, TargetPattern target)
      : bind_instruction(bind_instruction), target(target) {}

  template <typename T>
  bool match(T* m_value) {
    const auto branch = flugzeug::cast<Branch>(m_value);
    if (!branch) {
      return false;
    }

    const bool matched = target.match(branch->target());
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
  CondBranchOrSelectPattern(TInstruction** bind_instruction,
                            CondPattern cond,
                            TruePattern on_true,
                            FalsePattern on_false)
      : bind_instruction(bind_instruction), cond(cond), on_true(on_true), on_false(on_false) {}

  template <typename T>
  bool match(T* m_value) {
    const auto instruction = flugzeug::cast<std::remove_cv_t<TInstruction>>(m_value);
    if (!instruction) {
      return false;
    }

    bool matched;
    if constexpr (std::is_same_v<TInstruction, CondBranch>) {
      matched = cond.match(instruction->condition()) && on_true.match(instruction->true_target()) &&
                on_false.match(instruction->false_target());
    } else {
      matched = cond.match(instruction->condition()) && on_true.match(instruction->true_value()) &&
                on_false.match(instruction->false_value());
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

template <typename TInstruction>
class RetVoidPattern {
  TInstruction** bind_instruction;

 public:
  explicit RetVoidPattern(TInstruction** bind_instruction) : bind_instruction(bind_instruction) {}

  template <typename T>
  bool match(T* m_value) {
    const auto ret = flugzeug::cast<Ret>(m_value);
    if (!ret || ret->value()) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = ret;
    }

    return true;
  }
};

template <typename TInstruction, typename ValuePattern>
class RetPattern {
  TInstruction** bind_instruction;
  ValuePattern value;

 public:
  RetPattern(TInstruction** bind_instruction, ValuePattern value)
      : bind_instruction(bind_instruction), value(value) {}

  template <typename T>
  bool match(T* m_value) {
    const auto ret = flugzeug::cast<Ret>(m_value);
    if (!ret || !ret->value()) {
      return false;
    }

    const bool matched = value.match(ret->value());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = ret;
    }

    return true;
  }
};

template <typename TInstruction, typename BasePattern, typename IndexPattern>
class OffsetPattern {
  TInstruction** bind_instruction;
  BasePattern base;
  IndexPattern index;

 public:
  OffsetPattern(TInstruction** bind_instruction, BasePattern base, IndexPattern index)
      : bind_instruction(bind_instruction), base(base), index(index) {}

  template <typename T>
  bool match(T* m_value) {
    const auto offset = flugzeug::cast<Offset>(m_value);
    if (!offset) {
      return false;
    }

    const bool matched = base.match(offset->base()) && index.match(offset->index());
    if (!matched) {
      return false;
    }

    if (bind_instruction) {
      *bind_instruction = offset;
    }

    return true;
  }
};

}  // namespace detail

template <typename TInstruction, typename AddressPattern>
auto load(TInstruction*& instruction, AddressPattern address) {
  static_assert(std::is_same_v<Load, std::remove_cv_t<TInstruction>>,
                "Expected Load instruction in this pattern");
  return detail::LoadPattern<TInstruction, AddressPattern>(&instruction, address);
}

template <typename AddressPattern>
auto load(AddressPattern address) {
  return detail::LoadPattern<const Load, AddressPattern>(nullptr, address);
}

template <typename TInstruction, typename AddressPattern, typename ValuePattern>
auto store(TInstruction*& instruction, AddressPattern address, ValuePattern value) {
  static_assert(std::is_same_v<Store, std::remove_cv_t<TInstruction>>,
                "Expected Store instruction in this pattern");
  return detail::StorePattern<TInstruction, AddressPattern, ValuePattern>(&instruction, address,
                                                                          value);
}

template <typename AddressPattern, typename ValuePattern>
auto store(AddressPattern address, ValuePattern value) {
  return detail::StorePattern<const Store, AddressPattern, ValuePattern>(nullptr, address, value);
}

template <typename TInstruction, typename CalleePattern>
auto call(TInstruction*& instruction, CalleePattern callee) {
  static_assert(std::is_same_v<Call, std::remove_cv_t<TInstruction>>,
                "Expected Call instruction in this pattern");
  return detail::CallPattern<TInstruction, CalleePattern>(&instruction, callee);
}

template <typename CalleePattern>
auto call(CalleePattern callee) {
  return detail::CallPattern<const Call, CalleePattern>(nullptr, callee);
}

template <typename TInstruction, typename TargetPattern>
auto branch(TInstruction*& instruction, TargetPattern target) {
  static_assert(std::is_same_v<Branch, std::remove_cv_t<TInstruction>>,
                "Expected Branch instruction in this pattern");
  return detail::BranchPattern<TInstruction, TargetPattern>(&instruction, target);
}

template <typename TargetPattern>
auto branch(TargetPattern target) {
  return detail::BranchPattern<const Branch, TargetPattern>(nullptr, target);
}

template <typename TInstruction, typename CondPattern, typename TruePattern, typename FalsePattern>
auto cond_branch(TInstruction*& instruction,
                 CondPattern cond,
                 TruePattern on_true,
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

template <typename TInstruction>
auto stackalloc(TInstruction*& instruction) {
  static_assert(std::is_same_v<StackAlloc, std::remove_cv_t<TInstruction>>,
                "Expected StackAlloc instruction in this pattern");
  return detail::ValueBindingPattern<TInstruction>(&instruction);
}
inline auto stackalloc() {
  return detail::ValueBindingPattern<const StackAlloc>(nullptr);
}

template <typename TInstruction>
auto ret_void(TInstruction*& instruction) {
  static_assert(std::is_same_v<Ret, std::remove_cv_t<TInstruction>>,
                "Expected Ret instruction in this pattern");
  return detail::RetVoidPattern<TInstruction>(&instruction);
}
inline auto ret_void() {
  return detail::RetVoidPattern<const Ret>(nullptr);
}

template <typename TInstruction, typename ValuePattern>
auto ret(TInstruction*& instruction, ValuePattern value) {
  static_assert(std::is_same_v<Ret, std::remove_cv_t<TInstruction>>,
                "Expected Ret instruction in this pattern");
  return detail::RetPattern<TInstruction, ValuePattern>(&instruction, value);
}

template <typename ValuePattern>
auto ret(ValuePattern value) {
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
auto select(TInstruction*& instruction,
            CondPattern cond,
            TruePattern on_true,
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

template <typename TInstruction>
auto phi(TInstruction*& instruction) {
  static_assert(std::is_same_v<Phi, std::remove_cv_t<TInstruction>>,
                "Expected Phi instruction in this pattern");
  return detail::ValueBindingPattern<TInstruction>(&instruction);
}
inline auto phi() {
  return detail::ValueBindingPattern<const Phi>(nullptr);
}

}  // namespace flugzeug::pat