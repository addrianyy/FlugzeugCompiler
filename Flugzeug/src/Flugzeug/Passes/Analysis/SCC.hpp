#pragma once
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Flugzeug/Core/Error.hpp>

namespace flugzeug::analysis {

template <typename T>
struct SccContext {
  struct VertexData {
    std::optional<size_t> index;
    size_t lowlink = ~(size_t(0));
    bool on_stack = false;
  };

  size_t index = 0;
  std::vector<VertexData> vertices;
  std::vector<T> stack;

  std::vector<std::vector<T>> sccs;
  std::unordered_map<T, size_t> indices;
};

namespace detail {
template <typename T, bool SkipTrivialNonLoopingSCCs, typename Fn>
static void scc_visit(SccContext<T>& context,
                      T value,
                      const std::unordered_set<T>& value_set,
                      const Fn& get_neighbours) {
  const auto get_vertex = [&](T value) -> typename SccContext<T>::VertexData& {
    const auto it = context.indices.find(value);
    verify(it != context.indices.end(), "Value has no assigned index (?)");

    return context.vertices[it->second];
  };

  // Skip the value if it was already visited.
  if (get_vertex(value).index) {
    return;
  }

  auto& current_vertex = get_vertex(value);
  current_vertex.index = context.index;
  current_vertex.lowlink = context.index;
  current_vertex.on_stack = true;

  context.stack.push_back(value);
  context.index += 1;

  for (auto other_non_casted : get_neighbours(value)) {
    auto other = T(other_non_casted);
    if (!value_set.contains(other)) {
      continue;
    }

    auto& other_vertex = get_vertex(other);

    if (const auto other_index = other_vertex.index) {
      if (other_vertex.on_stack) {
        current_vertex.lowlink = std::min(current_vertex.lowlink, *other_index);
      }
    } else {
      scc_visit<T, SkipTrivialNonLoopingSCCs, Fn>(context, other, value_set, get_neighbours);

      current_vertex.lowlink = std::min(current_vertex.lowlink, other_vertex.lowlink);
    }
  }

  if (const auto vertex_index = current_vertex.index) {
    if (current_vertex.lowlink == *vertex_index) {
      std::vector<T> current_scc;

      while (true) {
        const auto current_value = context.stack.back();
        context.stack.pop_back();

        get_vertex(current_value).on_stack = false;

        current_scc.push_back(current_value);

        if (value == current_value) {
          break;
        }
      }

      if constexpr (SkipTrivialNonLoopingSCCs) {
        // Skip single-value SCCs where value isn't connected to itself.
        if (current_scc.size() == 1) {
          bool valid = false;

          const auto scc_value = current_scc[0];

          for (auto other : get_neighbours(scc_value)) {
            if (other == scc_value) {
              valid = true;
              break;
            }
          }

          if (!valid) {
            return;
          }
        }
      }

      context.sccs.push_back(std::move(current_scc));
    }
  }
}
}  // namespace detail

template <typename T, bool SkipTrivialNonLoopingSCCs, typename Fn>
std::vector<std::vector<T>> calculate_sccs(SccContext<T>& context,
                                           const std::unordered_set<T>& values,
                                           const Fn& get_neighbours) {
  // Clear the context.
  context.index = 0;
  context.vertices.clear();
  context.stack.clear();
  context.sccs.clear();
  context.indices.clear();

  // Assign index to every value.
  {
    uint32_t next_index = 0;
    for (T value : values) {
      context.indices.insert({value, next_index++});
    }
  }

  context.vertices.resize(values.size());

  for (T value : values) {
    detail::scc_visit<T, SkipTrivialNonLoopingSCCs, Fn>(context, value, values, get_neighbours);
  }

  verify(context.stack.empty(), "SCC stack is not empty at the end of the calculation");

  return std::move(context.sccs);
}

template <typename T, bool SkipTrivialNonLoopingSCCs, typename Fn>
std::vector<std::vector<T>> calculate_sccs(const std::unordered_set<T>& values,
                                           const Fn& get_neighbours) {
  SccContext<T> context{};
  return calculate_sccs<T, SkipTrivialNonLoopingSCCs, Fn>(context, values, get_neighbours);
}

}  // namespace flugzeug::analysis