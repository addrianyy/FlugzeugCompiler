#pragma once
#include <Flugzeug/IR/Block.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace flugzeug {

class Loop {
public:
  Block* header = nullptr;
  std::unordered_set<Block*> blocks;

  std::unordered_set<Block*> back_edges_from;
  std::vector<std::pair<Block*, Block*>> exiting_edges;

  std::vector<std::unique_ptr<Loop>> sub_loops;
};

std::vector<std::unique_ptr<Loop>> analyze_function_loops(Function* function);

} // namespace flugzeug