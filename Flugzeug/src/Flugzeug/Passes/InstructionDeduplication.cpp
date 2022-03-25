#include "InstructionDeduplication.hpp"

#include <Flugzeug/Core/TinyVector.hpp>

#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include "Analysis/Paths.hpp"
#include "Analysis/PointerAliasing.hpp"

#include <span>

using namespace flugzeug;

using InstructionUniqueIdentifier = TinyVector<uintptr_t, 3>;

struct InstructionUniqueIdentifierHash {
  size_t operator()(const InstructionUniqueIdentifier& identifier) const {
    size_t hash = 0;

    for (const auto element : identifier) {
      hash_combine(hash, element);
    }

    return hash;
  }
};

class UniqueIdentifierVisitor : public ConstInstructionVisitor {
  InstructionUniqueIdentifier& identifier;

public:
  explicit UniqueIdentifierVisitor(InstructionUniqueIdentifier& identifier)
      : identifier(identifier) {}

  void visit_unary_instr(Argument<UnaryInstr> unary) {
    identifier.push_back(uintptr_t(unary->get_op()));
  }
  void visit_binary_instr(Argument<BinaryInstr> binary) {
    identifier.push_back(uintptr_t(binary->get_op()));
  }
  void visit_int_compare(Argument<IntCompare> int_compare) {
    identifier.push_back(uintptr_t(int_compare->get_pred()));
  }
  void visit_load(Argument<Load> load) {}
  void visit_store(Argument<Store> store) {}
  void visit_call(Argument<Call> call) {}
  void visit_branch(Argument<Branch> branch) {}
  void visit_cond_branch(Argument<CondBranch> cond_branch) {}
  void visit_stackalloc(Argument<StackAlloc> stackalloc) {
    identifier.push_back(uintptr_t(stackalloc->get_size()));
  }
  void visit_ret(Argument<Ret> ret) {}
  void visit_offset(Argument<Offset> offset) {}
  void visit_cast(Argument<Cast> cast) { identifier.push_back(uintptr_t(cast->get_cast_kind())); }
  void visit_select(Argument<Select> select) {}
  void visit_phi(Argument<Phi> phi) {}
};

static InstructionUniqueIdentifier calculate_unique_identifier(const Instruction* instruction) {
  InstructionUniqueIdentifier identifier;

  for (const Value& operand : instruction->operands()) {
    identifier.push_back(uintptr_t(&operand));
  }

  UniqueIdentifierVisitor visitor(identifier);
  visitor::visit_instruction(instruction, visitor);

  return identifier;
}

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

    for (Instruction& instruction : advance_early(block)) {
      if (!can_be_deduplicated(&instruction)) {
        continue;
      }

      auto& map = deduplication_map[instruction.get_kind()];
      auto identifier = calculate_unique_identifier(&instruction);

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

    auto identifier = calculate_unique_identifier(&instruction);
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

  for (Instruction& instruction : advance_early(function->instructions())) {
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