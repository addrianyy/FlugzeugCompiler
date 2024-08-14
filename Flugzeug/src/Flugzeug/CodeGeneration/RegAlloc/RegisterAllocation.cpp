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

template <typename Container, typename Fn>
void for_each_erase(Container& container, Fn callback) {
  auto iterator = begin(container);
  while (iterator != end(container)) {
    if (callback(*iterator)) {
      iterator = container.erase(iterator);
    } else {
      ++iterator;
    }
  }
}

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
      Instruction* previous = phi.previous();

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

static bool generate_phi_moves(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    for (Phi& phi : advance_early(block.instructions<Phi>())) {
      for (const auto incoming : phi) {
        const auto move = new BinaryInstr(function->get_context(), incoming.value, BinaryOp::Add,
                                          incoming.value->get_type()->zero());
        move->insert_before(incoming.block->get_last_instruction());

        phi.replace_incoming_for_block(incoming.block, move);

        did_something = true;
      }
    }
  }

  return did_something;
}

static void prepare_function_for_regalloc(Function* function) {
  order_phis(function);

  const bool splitted = split_critical_edges(function);

  generate_phi_moves(function);

  if (splitted) {
    opt::CFGSimplification::run(function);
  }
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
                                 const std::vector<Block*>& toposort,
                                 const BackEdges& back_edges) {
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
    if (!instruction.has_value() || cast<Phi>(instruction.get()) ||
        instruction.get()->operand_count() == 0) {
      continue;
    }

    const auto operand = cast<Instruction>(instruction.get()->operand(0));
    if (operand) {
      const auto result = instruction.join_to(ordered_instructions.get(operand));

      if (false) {
        log_debug("Joining {} with {}: {}", instruction.get()->format(), operand->format(), result);
      }
    }
  }
}

static std::unordered_map<OrderedInstruction*, uint32_t> linear_scan_allocation(
  OrderedInstructions& ordered_instructions) {
  // Intervals which aren't processed yet.
  std::vector<OrderedInstruction*> unhandled;

  // Intervals which overlap `current.start`.
  std::vector<OrderedInstruction*> active;

  // Intervals which have holes and `current.start` falls into one of them.
  std::vector<OrderedInstruction*> inactive;

  std::unordered_map<OrderedInstruction*, uint32_t> registers;
  const auto get_register = [&](OrderedInstruction* instruction) {
    const auto it = registers.find(instruction);
    verify(it != registers.end(), "Register is not assigned yet (?)");
    return it->second;
  };

  std::unordered_set<uint32_t> free;
  std::unordered_set<uint32_t> tmp_free;

  uint32_t next_register_id = 0;

  // Sort all representative instructions so the last instruction in the list will have lowest
  // starting index.
  {
    unhandled.reserve(ordered_instructions.instructions().size());

    for (auto& instruction : ordered_instructions.instructions()) {
      if (!instruction.has_value() || instruction.is_joined()) {
        continue;
      }

      unhandled.push_back(&instruction);
    }

    std::sort(begin(unhandled), end(unhandled), [&](OrderedInstruction* a, OrderedInstruction* b) {
      return a->get_live_interval().first_range_start() >
             b->get_live_interval().first_range_start();
    });
  }

  while (!unhandled.empty()) {
    // Get unhandled instruction with the lowest starting point.
    const auto current = unhandled.back();
    const auto& current_li = current->get_live_interval();
    unhandled.pop_back();

    // Check for active intervals that expired.
    for_each_erase(active, [&](OrderedInstruction* instruction) {
      const auto& instruction_li = instruction->get_live_interval();
      const auto instruction_reg = get_register(instruction);

      if (instruction_li.ends_before(current_li)) {
        // `instruction` has already ended so we have fully handled it. The register it was using is
        // now free.
        free.insert(instruction_reg);

        return true;
      } else if (!instruction_li.overlaps_with(current_li.first_range_start())) {
        // `current.start` lies in the hole of `instruction`. `instruction` becomes inactive for now
        // and its register is temporarily free.
        inactive.push_back(instruction);
        free.insert(instruction_reg);

        return true;
      }

      return false;
    });

    // Check for inactive intervals that expired or become reactivated.
    for_each_erase(inactive, [&](OrderedInstruction* instruction) {
      const auto& instruction_li = instruction->get_live_interval();
      const auto instruction_reg = get_register(instruction);

      if (instruction_li.ends_before(current_li)) {
        // `instruction` has already ended so we have fully handled it. The register it was using
        // is already in the free set.
        return true;
      } else if (instruction_li.overlaps_with(current_li.first_range_start())) {
        // `current.start` was in the hole of `instruction` before but now they overlap. Reactivate
        // it: move `instruction` to the active list and mark its register as non-free.
        active.push_back(instruction);
        free.erase(instruction_reg);

        return true;
      }

      return false;
    });

    // Collect available registers in `tmp_free`.
    tmp_free.clear();
    tmp_free.insert(begin(free), end(free));

    // Make sure we don't use any register that is in `inactive` list and overlaps the current
    // interval.
    for (const auto instruction : inactive) {
      if (LiveInterval::are_overlapping(instruction->get_live_interval(), current_li)) {
        tmp_free.erase(get_register(instruction));
      }
    }

    // Assign register to `current`. If there isn't any free register we will create a new one. If
    // there is one we will take the first one.
    uint32_t assigned_reg = 0;
    if (tmp_free.empty()) {
      assigned_reg = next_register_id++;
    } else {
      assigned_reg = *tmp_free.begin();
      free.erase(assigned_reg);
    }

    registers[current] = assigned_reg;

    // `current` is now an active interval.
    active.push_back(current);
  }

  return registers;
}

static void debug_print_allocation(
  OrderedInstructions& ordered_instructions,
  const std::unordered_map<OrderedInstruction*, uint32_t>& allocation) {
  DebugRepresentation dbg_repr(ordered_instructions);

  log_debug("Register allocation:");

  for (const auto [instruction, reg] : allocation) {
    log_debug("{}: R{}", dbg_repr.format(instruction), reg);
  }

  log_debug("");
}

static void debug_verify_allocation(
  OrderedInstructions& ordered_instructions,
  const std::unordered_map<OrderedInstruction*, uint32_t>& allocation) {
  for (OrderedInstruction& a : ordered_instructions.instructions()) {
    if (!a.has_value() || a.is_joined()) {
      continue;
    }

    for (OrderedInstruction& b : ordered_instructions.instructions()) {
      if (&a == &b || &a > &b || !b.has_value() || b.is_joined()) {
        continue;
      }

      const auto overlap =
        LiveInterval::are_overlapping(a.get_live_interval(), b.get_live_interval());
      if (overlap) {
        verify(allocation.find(&a)->second != allocation.find(&b)->second,
               "Instructions have overlapping intervals but the have assigned the same register");
      }
    }
  }
}

AllocatedRegisters flugzeug::allocate_registers(Function* function) {
  prepare_function_for_regalloc(function);

  const DominatorTree dominator_tree(function);
  const BackEdges back_edges(dominator_tree);

  const auto toposort = toposort_blocks(function, back_edges);

  OrderedInstructions ordered_instructions(toposort);

  build_live_intervals(ordered_instructions, toposort, back_edges);
  coalesce(ordered_instructions);

  const auto allocation = linear_scan_allocation(ordered_instructions);

  if (false) {
    debug_verify_allocation(ordered_instructions, allocation);
  }

  if (true) {
    ordered_instructions.debug_print();
    ordered_instructions.debug_print_intervals();
    ordered_instructions.debug_print_interference();

    debug_print_allocation(ordered_instructions, allocation);
  }

  std::unordered_map<const Instruction*, uint32_t> registers;
  registers.reserve(allocation.size());

  for (OrderedInstruction& instruction : ordered_instructions.instructions()) {
    if (!instruction.has_value()) {
      continue;
    }

    const auto it = allocation.find(instruction.get_representative());
    verify(it != allocation.end(), "Not all registers were assigned during register allocation");

    registers.insert({instruction.get(), it->second});
  }

  return AllocatedRegisters(std::move(registers));
}

uint32_t AllocatedRegisters::get_register(const Instruction* instruction) const {
  const auto it = registers.find(instruction);
  verify(it == registers.end(), "No register was assigned to a given instruction");

  return it->second;
}