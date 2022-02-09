#include "KnownBitsOptimization.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

struct KnownBits {
  uint64_t mask = 0;
  uint64_t value = 0;

  static KnownBits combine(const KnownBits& kb1, const KnownBits& kb2) {
    // Get common mask for `kb1` and `kb2`.
    const auto common_mask = kb1.mask & kb2.mask;

    // Adjust known values with new common mask.
    const auto kb1_value = kb1.value & common_mask;
    const auto kb2_value = kb2.value & common_mask;

    // Create mask without differing known bits.
    const auto valid_mask = ~(kb1_value ^ kb2_value);

    return KnownBits{
      .mask = common_mask & valid_mask,
      .value = (kb1_value | kb2_value) & valid_mask,
    };
  }
};

class KnownBitsDatabase {
  std::unordered_map<Value*, KnownBits> known_bits;

public:
  KnownBits get(Value* value) {
    const auto type_bitmask = value->get_type()->get_bit_mask();

    if (const auto constant = cast<Constant>(value)) {
      return KnownBits{
        .mask = type_bitmask,
        .value = constant->get_constant_u(),
      };
    }

    if (const auto undef = cast<Undef>(value)) {
      return KnownBits{
        .mask = type_bitmask,
        .value = 0,
      };
    }

    const auto it = known_bits.find(value);
    if (it != known_bits.end()) {
      return it->second;
    }

    return KnownBits{};
  }

  void set(Value* value, const KnownBits& bits) {
    const auto type_bitmask = value->get_type()->get_bit_mask();

    verify((~bits.mask & bits.value) == 0 && (bits.mask & ~type_bitmask) == 0 &&
             (bits.value & ~type_bitmask) == 0,
           "Computed invalid known bits");

    known_bits[value] = bits;
  }
};

class BitOptimizer : InstructionVisitor {
public:
  KnownBitsDatabase& bits_database;
  bool did_something = false;

  explicit BitOptimizer(KnownBitsDatabase& bits_database) : bits_database(bits_database) {}

  void visit_binary_instr(Argument<BinaryInstr> binary) {}

  void visit_load(Argument<Load> load) {}
  void visit_store(Argument<Store> store) {}

  void visit_cond_branch(Argument<CondBranch> cond_branch) {}

  void visit_select(Argument<Select> select) {
    const auto bits = KnownBits::combine(bits_database.get(select->get_true_val()),
                                         bits_database.get(select->get_false_val()));
    bits_database.set(select, bits);
  }

  void visit_branch(Argument<Branch> branch) {}
  void visit_int_compare(Argument<IntCompare> int_compare) {}
  void visit_offset(Argument<Offset> offset) {}
  void visit_unary_instr(Argument<UnaryInstr> unary) {}
  void visit_call(Argument<Call> call) {}
  void visit_stackalloc(Argument<StackAlloc> stackalloc) {}
  void visit_ret(Argument<Ret> ret) {}

  void visit_cast(Argument<Cast> cast) {}

  void visit_phi(Argument<Phi> phi) {
    std::optional<KnownBits> bits;

    for (const auto incoming : *phi) {
      const auto incoming_bits = bits_database.get(incoming.value);
      bits = bits ? KnownBits::combine(*bits, incoming_bits) : incoming_bits;
    }

    bits_database.set(phi, *bits);
  }
};

bool KnownBitsOptimization::run(Function* function) {
  // We need to traverse blocks in the DFS order.
  const auto blocks =
    function->get_entry_block()->get_reachable_blocks(TraversalType::DFS_WithStart);

  KnownBitsDatabase bits_database;

  for (Block* block : blocks) {
    for (Instruction& instruction : *block) {
      BitOptimizer optimizer(bits_database);
      visitor::visit_instruction(&instruction, optimizer);
    }
  }

  return false;
}