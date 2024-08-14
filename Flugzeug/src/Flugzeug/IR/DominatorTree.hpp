#pragma once
#include <unordered_map>

namespace flugzeug {

class Block;
class Function;
class Instruction;

class DominatorTree {
  friend class Block;

  std::unordered_map<const Block*, const Block*> immediate_dominators;

  bool first_dominates_second(const Block* dominator, const Block* block) const;

 public:
  explicit DominatorTree(const Function* function);

  bool is_block_dead(const Block* block) const;
  const Block* immediate_dominator(const Block* block) const;
};

}  // namespace flugzeug