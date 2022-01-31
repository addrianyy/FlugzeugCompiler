#pragma once
#include <Flugzeug/IR/Block.hpp>

#include <unordered_map>
#include <vector>

namespace flugzeug {

struct SccContext {
  struct VertexData {
    std::optional<size_t> index;
    size_t lowlink = ~(size_t(0));
    bool on_stack = false;
  };

  size_t index = 0;
  std::vector<VertexData> vertices;
  std::vector<Block*> stack;

  std::vector<std::vector<Block*>> sccs;
  std::unordered_map<Block*, size_t> indices;
};

std::vector<std::vector<Block*>> calculate_sccs(SccContext& context,
                                                const std::unordered_set<Block*>& blocks);
std::vector<std::vector<Block*>> calculate_sccs(const std::unordered_set<Block*>& blocks);

} // namespace flugzeug