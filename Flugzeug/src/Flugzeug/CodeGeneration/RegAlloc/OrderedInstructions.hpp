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

  explicit LiveInterval(std::vector<Range> ranges) : ranges(std::move(ranges)) {}

public:
  LiveInterval() = default;

  std::span<const Range> get_ranges() const { return ranges; }

  static bool are_overlapping(const LiveInterval& a, const LiveInterval& b);
  static LiveInterval merge(const LiveInterval& a, const LiveInterval& b);

  void clear();
  void add(Range range);
};

class OrderedInstruction {
  size_t index = 0;
  Instruction* instruction = nullptr;

  LiveInterval live_interval;
  OrderedInstruction* representative = nullptr;

public:
  OrderedInstruction() = default;

  OrderedInstruction(size_t index, Instruction* instruction)
      : index(index), instruction(instruction) {}

  bool has_value() const;
  bool is_joined() const;

  size_t get_index() const { return index; }
  Instruction* get() const { return instruction; }

  OrderedInstruction* get_representative();
  const LiveInterval& get_live_interval();

  void add_live_range(Range range);
  bool join_to(OrderedInstruction* other);
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
  void debug_print_interference();
};

} // namespace flugzeug
