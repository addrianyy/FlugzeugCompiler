#pragma once

namespace flugzeug {

class Function;
class Block;

class CFGSimplification {
  static Block* get_intermediate_block_target(Block* block);
  static bool do_phis_depend_on_predecessors(const Block* block, const Block* p1, const Block* p2);
  static Block* thread_jump(Block* block, Block* target, bool* did_something);

  static bool thread_jumps(Function* function);
  static bool merge_blocks(Function* function);

public:
  static bool run(Function* function);
};

} // namespace flugzeug