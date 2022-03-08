#include "InstructionDeduplication.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include "Analysis/PointerAliasing.hpp"

using namespace flugzeug;

static bool can_be_deduplicated(Instruction* instruction) {
  if (instruction->is_volatile()) {
    return false;
  }

  switch (instruction->get_kind()) {
  case Value::Kind::StackAlloc:
  case Value::Kind::Phi:
    return false;

  default:
    return true;
  }
}

static bool deduplicate_block_local(Function* function) {
  bool did_something = false;

  analysis::PointerAliasing alias_analysis(function);
  std::unordered_map<Value::Kind, std::unordered_map<InstructionUniqueIdentifier, Instruction*,
                                                     InstructionUniqueIdentifierHash>>
    deduplication_map;

  for (Block& block : *function) {
    for (auto& [key, value] : deduplication_map) {
      value.clear();
    }

    for (Instruction& instruction : dont_invalidate_current(block)) {
      if (!can_be_deduplicated(&instruction)) {
        continue;
      }

      auto& map = deduplication_map[instruction.get_kind()];
      auto identifier = instruction.calculate_unique_identifier();

      if (const auto it = map.find(identifier); it == map.end()) {
        map.insert({std::move(identifier), &instruction});
      } else {
        const auto previous = it->second;

        if (const auto load = cast<Load>(instruction)) {
          // Special care needs to be taken if we want to deduplicate load. Something inbetween two
          // instructions may have modified loaded ptr and output value will be different.
          const auto stored_to_inbetween = alias_analysis.is_pointer_accessed_inbetween(
            load->get_ptr(), previous->get_next(), &instruction,
            analysis::PointerAliasing::AccessType::Store);

          if (stored_to_inbetween) {
            map.insert({std::move(identifier), &instruction});
            continue;
          }
        }

        instruction.replace_uses_with_and_destroy(previous);
        did_something = true;
      }
    }
  }

  return did_something;
}

static bool deduplicate_global(Function* function) { fatal_error("TODO"); }

bool opt::InstructionDeduplication::run(Function* function, OptimizationLocality locality) {
  switch (locality) {
  case OptimizationLocality::BlockLocal:
    return deduplicate_block_local(function);

  case OptimizationLocality::Global:
    return deduplicate_global(function);

  default:
    unreachable();
  }
}