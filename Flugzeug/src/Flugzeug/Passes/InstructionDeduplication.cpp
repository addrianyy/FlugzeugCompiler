#include "InstructionDeduplication.hpp"

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include "Analysis/Paths.hpp"
#include "Analysis/PointerAliasing.hpp"

#include <span>

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

  const analysis::PointerAliasing alias_analysis(function);
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

static bool deduplicate_global(Function* function) {
  std::unordered_map<Value::Kind,
                     std::unordered_map<InstructionUniqueIdentifier, std::vector<Instruction*>,
                                        InstructionUniqueIdentifierHash>>
    deduplication_map;
  std::unordered_map<Instruction*, std::span<Instruction*>> fast_deduplication_map;

  for (Instruction& instruction : function->instructions()) {
    if (!can_be_deduplicated(&instruction)) {
      continue;
    }

    auto identifier = instruction.calculate_unique_identifier();
    deduplication_map[instruction.get_kind()][std::move(identifier)].push_back(&instruction);
  }

  for (auto& map : deduplication_map) {
    for (auto& entry : map.second) {
      auto& instructions = entry.second;
      fast_deduplication_map.reserve(instructions.size());

      for (const auto instruction : instructions) {
        fast_deduplication_map.insert({instruction, std::span<Instruction*>(instructions)});
      }
    }
  }

  std::unordered_set<Instruction*> deduplicated_instructions;

  const analysis::PointerAliasing alias_analysis(function);
  const DominatorTree dominator_tree(function);
  analysis::PathValidator path_validator;

  for (Instruction& instruction : dont_invalidate_current(function->instructions())) {
    if (!can_be_deduplicated(&instruction)) {
      continue;
    }

    const auto it = fast_deduplication_map.find(&instruction);
    if (it == fast_deduplication_map.end()) {
      continue;
    }
    const auto& other_instructions = it->second;

    Instruction* deduplicate_from = nullptr;
    size_t best_result = std::numeric_limits<size_t>::max();

    for (Instruction* other : other_instructions) {
      if (other == &instruction || deduplicated_instructions.contains(other)) {
        continue;
      }

      std::optional<size_t> result;
      if (const auto load = cast<Load>(instruction)) {
        result = path_validator.validate_path(
          dominator_tree, other, &instruction, analysis::PathValidator::MemoryKillTarget::End,
          [&](const Instruction* instruction) {
            return alias_analysis.can_instruction_access_pointer(
                     instruction, load->get_ptr(), analysis::PointerAliasing::AccessType::Store) ==
                   analysis::Aliasing::Never;
          });
      } else {
        result = path_validator.validate_path_count(dominator_tree, other, &instruction);
      }

      if (result && *result < best_result) {
        best_result = *result;
        deduplicate_from = other;
      }
    }

    if (deduplicate_from) {
      deduplicated_instructions.insert(&instruction);
      instruction.replace_uses_with_and_destroy(deduplicate_from);
    }
  }

  return !deduplicated_instructions.empty();
}

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