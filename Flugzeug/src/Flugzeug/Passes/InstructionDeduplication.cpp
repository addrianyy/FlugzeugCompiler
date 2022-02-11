#include "InstructionDeduplication.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>

using namespace flugzeug;

static bool can_be_deduplicated(Instruction* instruction) {
  if (instruction->is_volatile()) {
    return false;
  }

  switch (instruction->get_kind()) {
  case Value::Kind::StackAlloc:
  case Value::Kind::Phi:
  case Value::Kind::Load:
    return false;

  default:
    return true;
  }
}

static bool deduplicate_block_local(Function* function) {
  bool did_something = false;

  for (Block& block : *function) {
    std::unordered_map<Value::Kind, std::unordered_map<InstructionUniqueIdentifier, Instruction*,
                                                       InstructionUniqueIdentifierHash>>
      deduplication_map;

    for (Instruction& instruction : dont_invalidate_current(block)) {
      if (!can_be_deduplicated(&instruction)) {
        continue;
      }

      auto& map = deduplication_map[instruction.get_kind()];
      auto identifier = instruction.calculate_unique_identifier();

      if (const auto& it = map.find(identifier); it == map.end()) {
        map.insert({std::move(identifier), &instruction});
      } else {
        instruction.replace_uses_with_and_destroy(it->second);
        did_something = true;
      }
    }
  }

  return did_something;
}

static bool deduplicate_global(Function* function) { fatal_error("TODO"); }

bool InstructionDeduplication::run(Function* function, DeduplicationStrategy strategy) {
  switch (strategy) {
  case DeduplicationStrategy::BlockLocal:
    return deduplicate_block_local(function);

  case DeduplicationStrategy::Global:
    return deduplicate_global(function);

  default:
    unreachable();
  }
}
