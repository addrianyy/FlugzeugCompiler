#pragma once
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flugzeug {

class Function;
class Block;
class Instruction;

struct BlockInstructionsRange {
  size_t first;
  size_t last;

  BlockInstructionsRange(class OrderedInstructions& ordered_instructions, Block* block);
};

struct Range {
  size_t start;
  size_t end;
};

class LiveInterval {
  std::vector<Range> ranges;

public:
  std::span<const Range> get_ranges() const { return ranges; }

  void add(Range range);
};

class OrderedInstruction {
  size_t index = 0;
  Instruction* instruction = nullptr;

  LiveInterval live_interval;

public:
  OrderedInstruction() = default;

  OrderedInstruction(size_t index, Instruction* instruction)
      : index(index), instruction(instruction) {}

  bool has_value() const;

  size_t get_index() const { return index; }
  Instruction* get() const { return instruction; }

  LiveInterval& get_live_interval() { return live_interval; }
};

class OrderedInstructions {
  std::unique_ptr<OrderedInstruction[]> order;
  size_t instruction_count = 0;

  std::unordered_map<Instruction*, OrderedInstruction*> map;

public:
  std::span<OrderedInstruction> instructions() {
    return {order.get(), order.get() + instruction_count};
  }

  explicit OrderedInstructions(const std::vector<Block*>& toposort);

  OrderedInstruction* get(Instruction* instruction);

  void debug_print();
  void debug_print_intervals();
};

} // namespace flugzeug
