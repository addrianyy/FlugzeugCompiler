#include "OrderedInstructions.hpp"

#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsolePrinter.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

BlockInstructionsRange::BlockInstructionsRange(OrderedInstructions& ordered_instructions,
                                               Block* block) {
  // Skip Phi instructions.
  Instruction* first_instruction = block->get_first_instruction();
  while (cast<Phi>(first_instruction)) {
    first_instruction = first_instruction->get_next();
  }

  first = ordered_instructions.get(first_instruction)->get_index();
  last = ordered_instructions.get(block->get_last_instruction())->get_index();
}

void LiveInterval::add(Range range) {
  if (ranges.empty()) {
    ranges.push_back(range);
  } else {
    const auto last = ranges.back();
    verify(last.end <= range.start, "Unordered insertion to live interval");

    // Try to merge the last range.
    if (last.end == range.start) {
      ranges.back().end = range.end;
    } else {
      ranges.push_back(range);
    }
  }
}

bool LiveInterval::are_overlapping(const LiveInterval& a, const LiveInterval& b) {
  std::span<const Range> a_ranges = a.get_ranges();
  std::span<const Range> b_ranges = b.get_ranges();

  size_t previous_end = 0;

  const auto process_range = [&](std::span<const Range>& span) {
    const auto range = span[0];
    span = span.subspan(1);

    const bool overlap = range.start < previous_end;

    previous_end = range.end;

    return overlap;
  };

  while (!a_ranges.empty() || !b_ranges.empty()) {
    // If one of the ranges is empty then we can immediately return the result.
    if (a_ranges.empty()) {
      return process_range(b_ranges);
    }
    if (b_ranges.empty()) {
      return process_range(a_ranges);
    }

    const auto ar = a_ranges[0];
    const auto br = b_ranges[0];

    bool result = false;
    if (ar.start < br.start) {
      result = process_range(a_ranges);
    } else if (ar.start > br.start) {
      result = process_range(b_ranges);
    } else {
      if (ar.end < br.end) {
        result = process_range(a_ranges);
      } else {
        result = process_range(b_ranges);
      }
    }

    if (result) {
      return true;
    }
  }

  return false;
}

LiveInterval LiveInterval::merge(const LiveInterval& a, const LiveInterval& b) {
  std::span<const Range> a_ranges = a.get_ranges();
  std::span<const Range> b_ranges = b.get_ranges();

  std::vector<Range> result;

  const auto process_range = [&](std::span<const Range>& span) {
    const auto range = span[0];
    span = span.subspan(1);

    if (result.empty()) {
      result.push_back(range);
    } else {
      if (range.start <= result.back().end) {
        result.back().end = std::max(result.back().end, range.end);
      } else {
        result.push_back(range);
      }
    }
  };

  while (!a_ranges.empty() || !b_ranges.empty()) {
    if (a_ranges.empty()) {
      while (!b_ranges.empty()) {
        process_range(b_ranges);
      }
      break;
    }

    if (b_ranges.empty()) {
      while (!a_ranges.empty()) {
        process_range(a_ranges);
      }
      break;
    }

    const auto ar = a_ranges[0];
    const auto br = b_ranges[0];

    if (ar.start < br.start) {
      process_range(a_ranges);
    } else if (ar.start > br.start) {
      process_range(b_ranges);
    } else {
      if (ar.end < br.end) {
        process_range(a_ranges);
      } else {
        process_range(b_ranges);
      }
    }
  }

  return LiveInterval(std::move(result));
}

void LiveInterval::clear() { ranges.clear(); }

bool OrderedInstruction::has_value() const { return !instruction->is_void(); }
bool OrderedInstruction::is_joined() const {
  return representative != nullptr && representative != this;
}

OrderedInstruction* OrderedInstruction::get_representative() {
  if (!representative || representative == this) {
    return this;
  }

  return representative->get_representative();
}

const LiveInterval& OrderedInstruction::get_live_interval() {
  return get_representative()->live_interval;
}

void OrderedInstruction::add_live_range(Range range) {
  verify(!is_joined(), "Cannot add live range to joined value");

  live_interval.add(range);
}

bool OrderedInstruction::join_to(OrderedInstruction* other) {
  const auto this_i = get_representative();
  const auto other_i = other->get_representative();

  // Already joined - return true.
  if (this_i == other_i) {
    return true;
  }

  // Make sure that instruction live intervals don't overlap. If they do we cannot join them.
  if (LiveInterval::are_overlapping(this_i->get_live_interval(), other_i->get_live_interval())) {
    return false;
  }

  other_i->live_interval =
    LiveInterval::merge(this_i->get_live_interval(), other_i->get_live_interval());
  this_i->live_interval.clear();
  representative = other_i;

  return true;
}

OrderedInstructions::OrderedInstructions(const std::vector<Block*>& toposort) {
  {
    for (Block* block : toposort) {
      instruction_count += block->get_instruction_count();
    }
    order = std::make_unique<OrderedInstruction[]>(instruction_count);
    map.reserve(instruction_count);
  }

  {
    size_t index = 0;
    for (Block* block : toposort) {
      for (Instruction& instruction : *block) {
        const size_t current_index = index++;

        order[current_index] = OrderedInstruction{current_index, &instruction};
        map.insert({&instruction, &order[current_index]});
      }
    }
  }
}

OrderedInstruction* OrderedInstructions::get(Instruction* instruction) {
  const auto it = map.find(instruction);
  if (it == map.end()) {
    return nullptr;
  }

  return it->second;
}

class DebugRepresentation {
  std::unordered_map<OrderedInstruction*, std::vector<OrderedInstruction*>> represents;

public:
  explicit DebugRepresentation(OrderedInstructions& ordered_instructions) {
    for (OrderedInstruction& instruction : ordered_instructions.instructions()) {
      if (!instruction.has_value() || !instruction.is_joined()) {
        continue;
      }

      represents[instruction.get_representative()].push_back(&instruction);
    }
  }

  std::string format(OrderedInstruction* instruction) const {
    verify(!instruction->is_joined(), "Instruction is joined with other instruction");

    const auto it = represents.find(instruction);
    if (it == represents.end()) {
      return instruction->get()->format();
    }

    std::string result = "{" + instruction->get()->format();
    for (const auto other : it->second) {
      result += fmt::format(", {}", other->get()->format());
    }
    result += "}";

    return result;
  }
};

void OrderedInstructions::debug_print() {
  Block* current_block = nullptr;

  ConsolePrinter printer(ConsolePrinter::Variant::ColorfulIfSupported);

  for (const auto& instruction : instructions()) {
    const auto block = instruction.get()->get_block();

    if (block != current_block) {
      printer.create_line_printer().print(block, IRPrinter::Item::Colon);
      current_block = block;
    }

    {
      const auto prefix = fmt::format("{:>4}: ", instruction.get_index());
      printer.raw_write(prefix);
    }

    instruction.get()->print(printer);
  }
}

void OrderedInstructions::debug_print_intervals() {
  const DebugRepresentation dbg_repr(*this);

  log_debug("Live intervals:");

  for (OrderedInstruction& instruction : instructions()) {
    if (!instruction.has_value() || instruction.is_joined()) {
      continue;
    }

    std::string interval;

    for (auto range : instruction.get_live_interval().get_ranges()) {
      interval += fmt::format(" [{}, {})", range.start, range.end);
    }

    log_debug("{}: {}", dbg_repr.format(&instruction), interval);
  }

  log_debug("");
}

void OrderedInstructions::debug_print_interference() {
  const DebugRepresentation dbg_repr(*this);

  log_debug("Interference:");

  for (OrderedInstruction& a : instructions()) {
    if (!a.has_value() || a.is_joined()) {
      continue;
    }

    for (OrderedInstruction& b : instructions()) {
      if (&a == &b || &a > &b || !b.has_value() || b.is_joined()) {
        continue;
      }

      const auto overlap =
        LiveInterval::are_overlapping(a.get_live_interval(), b.get_live_interval());

      verify(LiveInterval::are_overlapping(b.get_live_interval(), a.get_live_interval()) == overlap,
             "Non symmetric result of `are_overlapping`");

      if (overlap) {
        log_debug("{} - {}", dbg_repr.format(&a), dbg_repr.format(&b));
      }
    }
  }

  log_debug("");
}