#pragma once

namespace flugzeug {

class Function;
class Block;

class DeadBlockElimination {
  static void destroy_dead_block(Block* block);

public:
  static bool run(Function* function);
};

} // namespace flugzeug