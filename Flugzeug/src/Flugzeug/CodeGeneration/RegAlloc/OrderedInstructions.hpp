#pragma once
#include "LiveInterval.hpp"

#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flugzeug {

class Function;
class Block;
class Instruction;

class OrderedInstructions;
class OrderedInstruction;

class DebugRepresentation {
  std::unordered_map<class OrderedInstruction*, std::vector<OrderedInstruction*>> represents;

public:
  explicit DebugRepresentation(OrderedInstructions& ordered_instructions);

  std::string format(OrderedInstruction* instruction) const;
};

struct BlockInstructionsRange {
  size_t first;
  size_t last;

  BlockInstructionsRange(class OrderedInstructions& ordered_instructions, Block* block);
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

  size_t get_index() const { return index; }
  Instruction* get() const { return instruction; }

  bool has_value() const;
  bool is_joined() const;

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
  explicit OrderedInstructions(const std::vector<Block*>& toposort);

  std::span<OrderedInstruction> instructions() {
    return {order.get(), order.get() + instruction_count};
  }

  OrderedInstruction* get(Instruction* instruction);

  void debug_print();
  void debug_print_intervals();
  void debug_print_interference();
};

} // namespace flugzeug
