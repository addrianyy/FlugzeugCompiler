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

bool OrderedInstruction::has_value() const { return !instruction->is_void(); }

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
  for (OrderedInstruction& instruction : instructions()) {
    if (!instruction.has_value()) {
      continue;
    }

    std::string interval;

    for (auto range : instruction.get_live_interval().get_ranges()) {
      interval += fmt::format(" [{}, {})", range.start, range.end);
    }

    log_debug("{}: {}", instruction.get()->format(), interval);
  }
}