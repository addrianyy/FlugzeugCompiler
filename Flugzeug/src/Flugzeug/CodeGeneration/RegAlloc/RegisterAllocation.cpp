#include "RegisterAllocation.hpp"
#include "OrderedInstructions.hpp"

#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsolePrinter.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <Flugzeug/Passes/CFGSimplification.hpp>

// https://link.springer.com/content/pdf/10.1007%2F3-540-45937-5_17.pdf

using namespace flugzeug;

using PhiMovesMap = std::unordered_map<Instruction*, Phi*>;

class BackEdges {
  const DominatorTree& dominator_tree;

public:
  explicit BackEdges(const DominatorTree& dominator_tree) : dominator_tree(dominator_tree) {}

  bool is_back_edge(const Block* from, const Block* to) const {
    // A back edge is a CFG edge whose target dominates its source.
    // TODO: Is it right for irreducible control flow?
    return to->dominates(from, dominator_tree);
  }
};

/// Move Phi instructions so they are all the at beginning of every block.
static bool order_phis(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    Phi* last_phi = nullptr;

    for (Phi& phi : advance_early(block.instructions<Phi>())) {
      Instruction* previous = phi.get_previous();

      if (previous && !cast<Phi>(previous)) {
        if (!last_phi) {
          phi.move_before(block.get_first_instruction());
        } else {
          phi.move_after(last_phi);
        }

        did_something = true;
      }

      last_phi = &phi;
    }
  }

  return did_something;
}

static bool split_critical_edges(Function* function) {
  // A critical edge is an edge which is neither the only edge leaving its source block, nor the
  // only edge entering its destination block.

  bool did_something = false;

  for (Block& block : advance_early(*function)) {
    // Try to find critical edges between `predecessors` and `block`.
    if (block.predecessors().size() <= 1) {
      continue;
    }

    const auto has_critical_edge = any_of(block.predecessors(), [&](Block* predecessor) {
      return predecessor->successors().size() > 1;
    });
    if (!has_critical_edge) {
      continue;
    }

    // There is at least one critical edge between `predecessors` and `block`.
    const auto predecessors = std::vector(begin(block.predecessors()), end(block.predecessors()));
    for (Block* predecessor : predecessors) {
      if (predecessor->successors().size() <= 1) {
        continue;
      }

      // Edge between `predecessor` and `block` is a critical edge. Split it.

      const auto mid_block = function->create_block();
      mid_block->push_instruction_front(new Branch(function->get_context(), &block));

      predecessor->get_last_instruction()->replace_operands(&block, mid_block);
      block.replace_incoming_blocks_in_phis(predecessor, mid_block);

      did_something = true;
    }
  }

  return did_something;
}

static PhiMovesMap generate_phi_moves(Function* function) {
  PhiMovesMap moves_map;

  for (Block& block : *function) {
    for (Phi& phi : advance_early(block.instructions<Phi>())) {
      moves_map.reserve(moves_map.size() + phi.get_incoming_count());

      for (const auto incoming : phi) {
        const auto move = new BinaryInstr(function->get_context(), incoming.value, BinaryOp::Add,
                                          incoming.value->get_type()->get_zero());
        move->insert_before(incoming.block->get_last_instruction());

        phi.replace_incoming_for_block(incoming.block, move);

        moves_map.insert({move, &phi});
      }
    }
  }

  return moves_map;
}

static PhiMovesMap prepare_function_for_regalloc(Function* function) {
  order_phis(function);

  const bool splitted = split_critical_edges(function);
  auto phi_moves = generate_phi_moves(function);

  if (splitted) {
    opt::CFGSimplification::run(function);
  }

  return std::move(phi_moves);
}

static std::vector<Block*> toposort_blocks(Function* function, const BackEdges& back_edges) {
  // A topological sort of a directed graph is a linear ordering of its vertices such that for every
  // directed edge uv from vertex u to vertex v, u comes before v in the ordering.

  // Because CFG may contain cycles we are skipping the back edges when computing topological
  // ordering.

  std::vector<Block*> stack;
  std::vector<Block*> sorted;
  std::unordered_set<Block*> visited;

  stack.reserve(function->get_block_count() / 4);
  sorted.reserve(function->get_block_count());
  visited.reserve(function->get_block_count());

  const auto are_predecessors_processed = [&](Block* block) {
    return all_of(block->predecessors(), [&](Block* predecessor) {
      if (visited.contains(predecessor)) {
        return true;
      }

      // We want to skip back edges.
      if (back_edges.is_back_edge(predecessor, block)) {
        return true;
      }

      return false;
    });
  };

  for (Block& block : *function) {
    if (are_predecessors_processed(&block)) {
      stack.push_back(&block);
    }
  }

  while (!stack.empty()) {
    const auto block = stack.back();
    stack.pop_back();

    if (!visited.insert(block).second) {
      continue;
    }

    sorted.push_back(block);

    for (Block* successor : block->successors()) {
      if (!visited.contains(successor) && are_predecessors_processed(successor)) {
        stack.push_back(successor);
      }
    }
  }

  verify(sorted.size() == function->get_block_count(),
         "Topological sorting that skips back edges missed some blocks");

  return sorted;
}

static void build_live_intervals(OrderedInstructions& ordered_instructions,
                                 const std::vector<Block*>& toposort, const BackEdges& back_edges) {
  // Map: block -> values which are live at the beginning of the block
  std::unordered_map<Block*, std::unordered_set<OrderedInstruction*>> live_in_blocks;
  live_in_blocks.reserve(toposort.size());

  const auto process_successors_phis = [&](Block* block, Block* successor,
                                           std::unordered_set<OrderedInstruction*>& live) {
    for (Phi& phi : successor->instructions<Phi>()) {
      const auto incoming = cast<Instruction>(phi.get_incoming_by_block(block));
      verify(incoming, "Incoming value should be an instruction");

      // Make sure that successor's Phi isn't in the live set.
      live.erase(ordered_instructions.get(&phi));

      // Incoming value must live till the end of this block.
      live.insert(ordered_instructions.get(incoming));
    }
  };

  // Compute values which are live at the beginning of every block. We do this by iterating over
  // topological order in reverse. This means that when we process a given block all its successors
  // are already processed. The only exceptions are back edges but it's fine as values from back
  // edges need to be accessed via Phi instructions.
  for (Block* block : reversed(toposort)) {
    // Set of live values in the block.
    std::unordered_set<OrderedInstruction*> live;

    for (Block* successor : block->successors()) {
      // Everything that is live at the beginning of a successor needs to be live in this block too.
      if (const auto it = live_in_blocks.find(successor); it != live_in_blocks.end()) {
        live.insert(begin(it->second), end(it->second));
      } else {
        // If we haven't visited a successor yet then it must be a back edge.
        verify(back_edges.is_back_edge(block, successor),
               "Successor block wasn't visited yet and it's not a back edge");
      }

      process_successors_phis(block, successor, live);
    }

    // Add operands of the instructions in the block to the live list.
    // Remove instructions created in this block from the live set as these values aren't live
    // at the beginning of the block.
    for (Instruction& instruction : reversed(*block)) {
      // Skip Phis as they are treated specially.
      if (cast<Phi>(instruction)) {
        continue;
      }

      // This value was created in this block so it isn't live at the beginning of it.
      live.erase(ordered_instructions.get(&instruction));

      // All operands used in this instruction are live.
      for (Value& operand_v : instruction.operands()) {
        const auto operand = cast<Instruction>(operand_v);
        if (!operand) {
          continue;
        }

        live.insert(ordered_instructions.get(operand));
      }
    }

    // We have finished gathering all live values at the beginning of the block.
    live_in_blocks.insert({block, std::move(live)});
  }

  std::unordered_set<OrderedInstruction*> live;

  // Build the intervals.
  for (Block* block : toposort) {
    // Get index of first and last normal instruction in the block.
    const BlockInstructionsRange instructions_range(ordered_instructions, block);

    const auto add_last_use_in_block = [&](OrderedInstruction* instruction, size_t last_use) {
      const auto index = instruction->get_index();
      const auto created_in_block =
        index >= instructions_range.first && index <= instructions_range.last;

      // If used instruction is created in this block we will add range:
      //   [instruction.index, last_use)
      // otherwise we will add:
      //   [block.first_instruction, last_use)
      instruction->add_live_range(created_in_block ? Range{index, last_use}
                                                   : Range{instructions_range.first, last_use});
    };

    live.clear();

    // Gether values which are live at the beginning of the successors.
    for (Block* successor : block->successors()) {
      // Everything that is live at the beginning of a successor needs to be live in this block too.
      {
        const auto it = live_in_blocks.find(successor);
        verify(it != live_in_blocks.end(), "Map of live values in blocks is incomplete");
        live.insert(begin(it->second), end(it->second));
      }

      process_successors_phis(block, successor, live);
    }

    // All values which are live at the beginning of the successors must live till the end of this
    // block.
    for (OrderedInstruction* instruction : live) {
      if (cast<Phi>(instruction->get())) {
        continue;
      }

      add_last_use_in_block(instruction, instructions_range.last + 1);
    }

    // Add operands of the instructions in the block to the live list.
    for (Instruction& instruction : reversed(*block)) {
      // Skip Phis as they are treated specially.
      if (cast<Phi>(instruction)) {
        continue;
      }

      // As we are iterating in reverse order as soon as we encounter this instruction it isn't live
      // anymore.
      const auto instruction_o = ordered_instructions.get(&instruction);
      live.erase(instruction_o);

      for (Value& operand_v : instruction.operands()) {
        const auto operand = cast<Instruction>(operand_v);
        if (!operand) {
          continue;
        }

        const auto operand_o = ordered_instructions.get(operand);

        // If this operand isn't in the live set yet then it's the last use of this operand so far.
        if (live.insert(operand_o).second) {
          add_last_use_in_block(operand_o, instruction_o->get_index());
        }
      }
    }
  }
}

static void coalesce(OrderedInstructions& ordered_instructions) {
  // Make sure that all incoming values and Phis are mapped to the same register. It is required for
  // correctness.
  for (OrderedInstruction& instruction : ordered_instructions.instructions()) {
    const auto phi = cast<Phi>(instruction.get());
    if (!phi) {
      continue;
    }

    for (const auto incoming : *phi) {
      const auto incoming_instruction = cast<Instruction>(incoming.value);
      verify(incoming_instruction, "Phi incoming values should be instructions");
      verify(ordered_instructions.get(incoming_instruction)->join_to(&instruction),
             "Failed to coalesce Phi incoming values");
    }
  }

  // Try to coalesce instruction result with first argument. This is done for speed.
  for (OrderedInstruction& instruction : ordered_instructions.instructions()) {
    if (cast<Phi>(instruction.get()) || instruction.get()->get_operand_count() == 0) {
      continue;
    }

    const auto operand = cast<Instruction>(instruction.get()->get_operand(0));
    if (operand) {
      const auto result = instruction.join_to(ordered_instructions.get(operand));

      if (false) {
        log_debug("Joining {} with {}: {}", instruction.get()->format(), operand->format(), result);
      }
    }
  }
}

void flugzeug::allocate_registers(Function* function) {
  const auto phi_moves = prepare_function_for_regalloc(function);

  const DominatorTree dominator_tree(function);
  const BackEdges back_edges(dominator_tree);

  const auto toposort = toposort_blocks(function, back_edges);

  OrderedInstructions ordered_instructions(toposort);

  build_live_intervals(ordered_instructions, toposort, back_edges);
  coalesce(ordered_instructions);

  ordered_instructions.debug_print();
  ordered_instructions.debug_print_intervals();
  ordered_instructions.debug_print_interference();
  function->debug_graph();

  std::exit(0);
}