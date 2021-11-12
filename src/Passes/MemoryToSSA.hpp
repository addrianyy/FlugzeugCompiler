#pragma once
#include <vector>

namespace flugzeug {

class StackAlloc;
class Function;
class Block;
class Value;
class Phi;

class MemoryToSSA {
  static bool is_stackalloc_optimizable(const StackAlloc* stackalloc);
  static std::vector<StackAlloc*> find_optimizable_stackallocs(Function* function);
  static Value* get_value_for_first_use(Block* block, StackAlloc* stackalloc,
                                        std::vector<Phi*>& inserted_phis);
  static void optimize_stackalloc(StackAlloc* stackalloc);

public:
  static bool run(Function* function);
};

} // namespace flugzeug