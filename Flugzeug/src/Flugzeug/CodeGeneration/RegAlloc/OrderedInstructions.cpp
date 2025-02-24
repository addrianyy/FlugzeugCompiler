#include "OrderedInstructions.hpp"

#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsoleIRPrinter.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

DebugRepresentation::DebugRepresentation(OrderedInstructions& ordered_instructions) {
  for (OrderedInstruction& instruction : ordered_instructions.instructions()) {
    if (!instruction.has_value() || !instruction.is_joined()) {
      continue;
    }

    represents[instruction.representative()].push_back(&instruction);
  }
}

std::string DebugRepresentation::format(OrderedInstruction* instruction) const {
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

BlockInstructionsRange::BlockInstructionsRange(OrderedInstructions& ordered_instructions,
                                               Block* block) {
  // Skip Phi instructions.
  Instruction* first_instruction = block->first_instruction();
  while (cast<Phi>(first_instruction)) {
    first_instruction = first_instruction->next();
  }

  first = ordered_instructions.get(first_instruction)->index();
  last = ordered_instructions.get(block->last_instruction())->index();
}

bool OrderedInstruction::has_value() const {
  return !instruction->is_void();
}
bool OrderedInstruction::is_joined() const {
  return representative_ != nullptr && representative_ != this;
}

OrderedInstruction* OrderedInstruction::representative() {
  if (!representative_ || representative_ == this) {
    return this;
  }

  return representative_->representative();
}

const LiveInterval& OrderedInstruction::live_interval() {
  return representative()->live_interval_;
}

void OrderedInstruction::add_live_range(Range range) {
  verify(!is_joined(), "Cannot add live range to joined value");

  live_interval_.add(range);
}

bool OrderedInstruction::join_to(OrderedInstruction* other) {
  const auto this_i = representative();
  const auto other_i = other->representative();

  // Already joined - return true.
  if (this_i == other_i) {
    return true;
  }

  // Make sure that instruction live intervals don't overlap. If they do we cannot join them.
  if (LiveInterval::are_overlapping(this_i->live_interval(), other_i->live_interval())) {
    return false;
  }

  other_i->live_interval_ = LiveInterval::merge(this_i->live_interval(), other_i->live_interval());
  this_i->live_interval_.clear();
  representative_ = other_i;

  return true;
}

OrderedInstructions::OrderedInstructions(const std::vector<Block*>& toposort) {
  {
    for (Block* block : toposort) {
      instruction_count += block->instruction_count();
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

void OrderedInstructions::debug_print() {
  Block* current_block = nullptr;

  ConsoleIRPrinter printer(ConsoleIRPrinter::Variant::ColorfulIfSupported);

  for (const auto& instruction : instructions()) {
    const auto block = instruction.get()->block();

    if (block != current_block) {
      printer.create_line_printer().print(block, IRPrinter::Item::Colon);
      current_block = block;
    }

    {
      const auto prefix = fmt::format("{:>4}: ", instruction.index());
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

    for (const auto range : instruction.live_interval().ranges()) {
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

      const auto overlap = LiveInterval::are_overlapping(a.live_interval(), b.live_interval());

      verify(LiveInterval::are_overlapping(b.live_interval(), a.live_interval()) == overlap,
             "Non symmetric result of `are_overlapping`");

      if (overlap) {
        log_debug("{} -- {}", dbg_repr.format(&a), dbg_repr.format(&b));
      }
    }
  }

  log_debug("");
}