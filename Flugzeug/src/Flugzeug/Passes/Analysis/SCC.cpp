#include "SCC.hpp"

using namespace flugzeug;

static void scc_visit(SccContext& context, Block* block, const std::unordered_set<Block*>& blocks) {
  const auto get_vertex = [&](Block* block) -> SccContext::VertexData& {
    const auto it = context.indices.find(block);
    verify(it != context.indices.end(), "Block has no assigned index (?)");

    return context.vertices[it->second];
  };

  // Skip vertex if it was already visited.
  if (get_vertex(block).index) {
    return;
  }

  auto& current_vertex = get_vertex(block);
  current_vertex.index = context.index;
  current_vertex.lowlink = context.index;
  current_vertex.on_stack = true;

  context.stack.push_back(block);
  context.index += 1;

  for (const auto other : block->get_successors()) {
    if (!blocks.contains(other)) {
      continue;
    }

    auto& other_vertex = get_vertex(other);

    if (const auto other_index = other_vertex.index) {
      if (other_vertex.on_stack) {
        current_vertex.lowlink = std::min(current_vertex.lowlink, *other_index);
      }
    } else {
      scc_visit(context, other, blocks);

      current_vertex.lowlink = std::min(current_vertex.lowlink, other_vertex.lowlink);
    }
  }

  if (const auto vertex_index = current_vertex.index) {
    if (current_vertex.lowlink == *vertex_index) {
      std::vector<Block*> current_scc;

      while (true) {
        const auto current_block = context.stack.back();
        context.stack.pop_back();

        get_vertex(current_block).on_stack = false;

        current_scc.push_back(current_block);

        if (block == current_block) {
          break;
        }
      }

      if (current_scc.size() == 1) {
        bool valid = false;

        const auto scc_block = current_scc[0];

        for (const auto successor : scc_block->get_successors()) {
          if (successor == scc_block) {
            valid = true;
            break;
          }
        }

        if (!valid) {
          return;
        }
      }

      context.sccs.push_back(std::move(current_scc));
    }
  }
}

std::vector<std::vector<Block*>>
flugzeug::calculate_sccs(SccContext& context, const std::unordered_set<Block*>& blocks) {
  // Clear context.
  context.index = 0;
  context.vertices.clear();
  context.stack.clear();
  context.sccs.clear();
  context.indices.clear();

  // Assign index to every vertex.
  {
    uint32_t next_index = 0;
    for (const auto block : blocks) {
      context.indices.insert({block, next_index++});
    }
  }

  context.vertices.resize(blocks.size());

  for (const auto block : blocks) {
    scc_visit(context, block, blocks);
  }

  verify(context.stack.empty(), "SCC stack is not empty at the end of the calculation");

  return std::move(context.sccs);
}

std::vector<std::vector<Block*>>
flugzeug::calculate_sccs(const std::unordered_set<Block*>& blocks) {
  SccContext context{};
  return calculate_sccs(context, blocks);
}