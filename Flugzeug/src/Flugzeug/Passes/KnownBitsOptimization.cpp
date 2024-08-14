#include "KnownBitsOptimization.hpp"
#include "Utils/Evaluation.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionVisitor.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

struct KnownBits {
  uint64_t mask = 0;
  uint64_t value = 0;

  std::optional<bool> sign(Type* type) const {
    const size_t type_size = type->bit_size();

    if ((mask & (uint64_t(1) << (type_size - 1))) != 0) {
      return (value >> (type_size - 1)) != 0;
    }

    return std::nullopt;
  }
};

class KnownBitsDatabase {
  std::unordered_map<Value*, KnownBits> known_bits;

 public:
  KnownBits get(Value* value) {
    const auto type_bitmask = value->type()->bit_mask();

    if (const auto constant = cast<Constant>(value)) {
      return KnownBits{
        .mask = type_bitmask,
        .value = constant->value_u(),
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
    const auto type_bitmask = value->type()->bit_mask();

    verify((~bits.mask & bits.value) == 0 && (bits.mask & ~type_bitmask) == 0 &&
             (bits.value & ~type_bitmask) == 0,
           "Computed invalid known bits");

    known_bits[value] = bits;
  }
};

namespace bitops {

static KnownBits combine(const KnownBits& a, const KnownBits& b) {
  // Get common mask for `a` and `b`.
  const auto common_mask = a.mask & b.mask;

  // Adjust known values with new common mask.
  const auto a_value = a.value & common_mask;
  const auto b_value = b.value & common_mask;

  // Create mask without differing known bits.
  const auto valid_mask = ~(a_value ^ b_value);

  return KnownBits{
    .mask = common_mask & valid_mask,
    .value = (a_value | b_value) & valid_mask,
  };
}

static std::optional<bool> compare_greater(const KnownBits& a, const KnownBits& b, Type* type) {
  for (size_t ri = 0; ri < type->bit_size(); ++ri) {
    // Go through every bit from MSB to LSB.
    const size_t i = type->bit_size() - 1 - ri;
    const uint64_t m = uint64_t(1) << i;

    // If this bit is not known in both operands we cannot continue.
    if ((a.mask & m) == 0 || (b.mask & m) == 0) {
      break;
    }

    // Extract bit values.
    const bool bit_a = ((a.value >> i) & 1) != 0;
    const bool bit_b = ((b.value >> i) & 1) != 0;

    if (bit_a != bit_b) {
      // This is first bit from MSB that is different in `a` and `b`.
      // If `a` has this bit set it means that `a` is > `b`. Otherwise `b` > `a`.
      return bit_a;
    }
  }

  return std::nullopt;
}

static KnownBits add(const KnownBits& a, const KnownBits& b, Type* type) {
  KnownBits computed{};

  if (a.mask != 0 && b.mask != 0) {
    uint64_t carry = 0;

    for (size_t i = 0; i < type->bit_size(); ++i) {
      const uint64_t m = uint64_t(1) << i;

      // If this bit is not known in both operands we cannot continue.
      if ((a.mask & m) == 0 || (b.mask & m) == 0) {
        break;
      }

      const uint64_t a_bit = (a.value >> i) & 1;
      const uint64_t b_bit = (b.value >> i) & 1;
      const uint64_t result = a_bit + b_bit + carry;

      // Set this bit as known.
      computed.value |= (result & 1) << i;
      computed.mask |= m;

      carry = result / 2;
    }
  }

  return computed;
}

static KnownBits neg(const KnownBits& bits, Type* type) {
  // -a = ~a + 1

  // Invert all known bits.
  KnownBits inverted = bits;
  inverted.value = ~inverted.value & inverted.mask;

  const bool is_zero = inverted.mask == 0 || inverted.value == 0;

  // Add one to known bits.
  auto computed = add(inverted,
                      KnownBits{
                        .mask = type->bit_size(),
                        .value = 1,
                      },
                      type);

  const uint64_t sign_mask = uint64_t(1) << (type->bit_size() - 1);

  if (!is_zero && (computed.mask & sign_mask) == 0) {
    // If the value is not zero than change sign bit independently (if its not known).
    // TODO: This is not correct if overflow happens. Is it OK?
    if (const auto sign = inverted.sign(type)) {
      computed.mask |= sign_mask;

      // Sign is already properly inverted.
      if (*sign) {
        computed.value |= sign_mask;
      } else {
        computed.value &= !sign_mask;
      }
    }
  }

  return computed;
}

}  // namespace bitops

enum class BitOptimizationResult {
  CalculatedBits,
  ModifiedInstruction,
  DestroyedInstruction,
  Unchanged,
};

class BitOptimizer : InstructionVisitor {
 public:
  KnownBitsDatabase& bits_database;

  explicit BitOptimizer(KnownBitsDatabase& bits_database) : bits_database(bits_database) {}

  BitOptimizationResult visit_unary_instr(Argument<UnaryInstr> unary) {
    KnownBits computed{};

    switch (unary->get_op()) {
      case UnaryOp::Not: {
        // Invert all known bits.
        computed = bits_database.get(unary->get_val());
        computed.value = ~computed.value & computed.mask;
        break;
      }

      case UnaryOp::Neg: {
        computed = bitops::neg(bits_database.get(unary->get_val()), unary->type());
        break;
      }
    }

    bits_database.set(unary, computed);

    return BitOptimizationResult::CalculatedBits;
  }

  BitOptimizationResult visit_binary_instr(Argument<BinaryInstr> binary) {
    const auto type = binary->type();
    const auto op = binary->get_op();

    auto a = bits_database.get(binary->get_lhs());
    auto b = bits_database.get(binary->get_rhs());

    KnownBits computed{};

    switch (op) {
      case BinaryOp::Or:
      case BinaryOp::And:
      case BinaryOp::Xor: {
        // Get common `known` and `mask` for two operands.
        auto mask = a.mask & b.mask;
        auto value = utils::evaluate_binary_instr(type, a.value, op, b.value) & mask;

        // `and` and `or` can give us more information about known bits.
        if (op != BinaryOp::Xor) {
          // Check every bit.
          for (size_t i = 0; i < type->bit_size(); ++i) {
            const uint64_t m = uint64_t(1) << i;

            switch (op) {
              case BinaryOp::Or: {
                // If this bit is one in at least one operand then destination will be one too.
                const auto a_one = (a.value & m) != 0 && (a.mask & m) != 0;
                const auto b_one = (b.value & m) != 0 && (b.mask & m) != 0;

                if (a_one || b_one) {
                  mask |= m;
                  value |= m;
                }

                break;
              }

              case BinaryOp::And: {
                // If this bit is zero in at least one operand then destination will be zero too.
                const auto a_zero = (a.value & m) == 0 && (a.mask & m) != 0;
                const auto b_zero = (b.value & m) == 0 && (b.mask & m) != 0;

                if (a_zero || b_zero) {
                  mask |= m;
                  value &= ~m;
                }

                break;
              }

              default:
                unreachable();
            }
          }

          // If one operand is constant maybe we can prove that this operation is unneccesary.
          if (a.mask == type->bit_mask() || b.mask == type->bit_mask()) {
            uint64_t known_value = 0;
            Value* partial_value = nullptr;
            KnownBits partial_bits;

            // Get fully known value and partially known value.
            if (a.mask == type->bit_mask()) {
              known_value = a.value;
              partial_value = binary->get_rhs();
              partial_bits = b;
            } else if (b.mask == type->bit_mask()) {
              known_value = b.value;
              partial_value = binary->get_lhs();
              partial_bits = a;
            } else {
              unreachable();
            }

            bool can_be_optimized = false;

            // Check if this operation can affect the result. If it can't, optimize it out.
            switch (op) {
              case BinaryOp::Or: {
                // 1. Make sure that all 1 bits in `value` are known.
                // 2. Make sure that or doesn't affect known bits.
                if (((~partial_bits.mask & known_value) & type->bit_mask()) == 0 &&
                    (partial_bits.value | known_value) == partial_bits.value) {
                  can_be_optimized = true;
                }
                break;
              }

              case BinaryOp::And: {
                // 1. Make sure that all 0 bits in `value` are known.
                // 2. Make sure that and doesn't affect known bits.
                if (((~partial_bits.mask & ~known_value) & type->bit_mask()) == 0 &&
                    (partial_bits.value & known_value) == partial_bits.value) {
                  can_be_optimized = true;
                }
                break;
              }

              default:
                unreachable();
            }

            if (can_be_optimized) {
              binary->replace_uses_with_and_destroy(partial_value);
              return BitOptimizationResult::DestroyedInstruction;
            }
          }
        }

        computed.mask = mask;
        computed.value = value;

        break;
      }

      case BinaryOp::Shl:
      case BinaryOp::Shr:
      case BinaryOp::Sar: {
        const auto shift_amount_constant = cast<Constant>(binary->get_rhs());
        if (shift_amount_constant) {
          const auto shift_amount = shift_amount_constant->value_u();

          auto mask = a.mask;
          auto value = a.value;

          // Shift known bits by the specified amount.
          switch (op) {
            case BinaryOp::Shl: {
              mask <<= shift_amount;
              value <<= shift_amount;
              break;
            }

            case BinaryOp::Shr:
            case BinaryOp::Sar: {
              mask >>= shift_amount;
              value >>= shift_amount;
              break;
            }

            default:
              unreachable();
          }

          // Clear out of bounds bits.
          mask &= type->bit_mask();
          value &= type->bit_mask();

          if (shift_amount != 0) {
            // Some bits after shifting may become known. Calculate mask of shifted out bits.
            uint64_t left_shift_amount_mask = 0;
            for (size_t i = 0; i < shift_amount; ++i) {
              left_shift_amount_mask = uint64_t(1) << i;
            }
            const uint64_t right_shift_amount_mask = left_shift_amount_mask
                                                     << (type->bit_size() - shift_amount);

            switch (op) {
              case BinaryOp::Shl: {
                // All shifted out bits become zero.
                mask |= left_shift_amount_mask;
                value &= ~left_shift_amount_mask;
                break;
              }

              case BinaryOp::Shr: {
                // All shifted bits become zero.
                mask |= right_shift_amount_mask;
                value &= ~right_shift_amount_mask;
                break;
              }

              case BinaryOp::Sar: {
                // Bits become known only if `a` sign bit is known.
                const auto sign = a.sign(type);
                if (sign) {
                  mask |= right_shift_amount_mask;

                  // All shifted bits become equal to the sign of `a`.
                  if (*sign) {
                    value |= right_shift_amount_mask;
                  } else {
                    value &= ~right_shift_amount_mask;
                  }
                }

                break;
              }

              default:
                unreachable();
            }
          }

          computed.mask = mask;
          computed.value = value;
        }

        break;
      }

      case BinaryOp::Add:
      case BinaryOp::Sub: {
        if (op == BinaryOp::Sub) {
          // x - y => x + (-y)
          b = bitops::neg(b, type);
        }

        computed = bitops::add(a, b, type);

        break;
      }

      default:
        return BitOptimizationResult::Unchanged;
    }

    bits_database.set(binary, computed);

    return BitOptimizationResult::CalculatedBits;
  }

  BitOptimizationResult visit_select(Argument<Select> select) {
    const auto bits = bitops::combine(bits_database.get(select->get_true_val()),
                                      bits_database.get(select->get_false_val()));
    bits_database.set(select, bits);

    return BitOptimizationResult::CalculatedBits;
  }

  BitOptimizationResult visit_phi(Argument<Phi> phi) {
    std::optional<KnownBits> bits;

    for (const auto incoming : *phi) {
      const auto incoming_bits = bits_database.get(incoming.value);
      bits = bits ? bitops::combine(*bits, incoming_bits) : incoming_bits;
    }

    bits_database.set(phi, *bits);

    return BitOptimizationResult::CalculatedBits;
  }

  BitOptimizationResult visit_int_compare(Argument<IntCompare> int_compare) {
    const auto type = int_compare->get_lhs()->type();
    const auto pred = int_compare->get_pred();

    auto a = bits_database.get(int_compare->get_lhs());
    auto b = bits_database.get(int_compare->get_rhs());

    std::optional<bool> result;

    // Try to resolve `cmp` result using known bits.
    switch (pred) {
      case IntPredicate::Equal:
      case IntPredicate::NotEqual: {
        // Quickly prove inequality by comparing operands known bits.
        const auto common_mask = a.mask & b.mask;
        const auto not_equal = (a.value & common_mask) != (b.value & common_mask);

        // We can only prove inequality, we don't know if values
        // are equal.
        if (not_equal) {
          if (pred == IntPredicate::Equal) {
            result = false;
          } else {
            result = true;
          }
        }

        break;
      }

      case IntPredicate::GtS:
      case IntPredicate::GteS:
      case IntPredicate::LtS:
      case IntPredicate::LteS: {
        if (pred == IntPredicate::LtS || pred == IntPredicate::LteS) {
          std::swap(a, b);
        }

        const auto a_sign = a.sign(type);
        const auto b_sign = b.sign(type);

        if (a_sign && b_sign) {
          if (*a_sign != *b_sign) {
            // If `b` is negative than `a` is positive and `a` is always > and >= `b`.
            result = *b_sign;
          } else if (!*a_sign) {
            // Compare positive integers.
            result = bitops::compare_greater(a, b, type);
          } else {
            // Fake positive integers. Because of how `compare_greater` works it shouldn't affect
            // the results.
            a.value = ~a.value & a.mask;
            b.value = ~b.value & b.mask;

            result = bitops::compare_greater(a, b, type);
          }
        }

        break;
      }

      case IntPredicate::GtU:
      case IntPredicate::GteU: {
        result = bitops::compare_greater(a, b, type);
        break;
      }

      case IntPredicate::LtU:
      case IntPredicate::LteU: {
        result = bitops::compare_greater(b, a, type);
        break;
      }
    }

    // If comparison result is constant then replace `cmp` with that constant.
    if (result) {
      int_compare->replace_uses_with_constant_and_destroy(*result);
      return BitOptimizationResult::DestroyedInstruction;
    }

    return BitOptimizationResult::Unchanged;
  }

  BitOptimizationResult visit_cast(Argument<Cast> cast_instr) {
    const auto input_type = cast_instr->get_val()->type();
    const auto input_bitmask = input_type->bit_mask();

    const auto output_type = cast_instr->type();
    const auto output_bitmask = output_type->bit_mask();

    const auto input = bits_database.get(cast_instr->get_val());

    KnownBits computed{};

    switch (cast_instr->get_cast_kind()) {
      case CastKind::Truncate:
      case CastKind::Bitcast: {
        // Just carry over previous known value and mask off truncated part.
        computed = input;
        computed.mask &= output_bitmask;
        computed.value &= output_bitmask;
        break;
      }

      case CastKind::SignExtend:
      case CastKind::ZeroExtend: {
        std::optional<bool> extension_bit;

        // Try to get the value of extension bit.
        if (cast_instr->get_cast_kind() == CastKind::SignExtend) {
          extension_bit = input.sign(input_type);
        } else {
          extension_bit = false;
        }

        if (extension_bit) {
          // Calculate mask of all bits that will be set to `extension_bit`.
          const uint64_t extension_mask = output_bitmask & ~input_bitmask;

          computed.mask |= extension_mask;

          if (*extension_bit) {
            computed.value |= extension_mask;
          } else {
            computed.value &= ~extension_mask;
          }
        }

        computed.value |= input.value;
        computed.mask |= input.mask;

        break;
      }
    }

    bits_database.set(cast_instr, computed);

    return BitOptimizationResult::CalculatedBits;
  }

  BitOptimizationResult visit_load(Argument<Load> load) { return BitOptimizationResult::Unchanged; }
  BitOptimizationResult visit_store(Argument<Store> store) {
    return BitOptimizationResult::Unchanged;
  }
  BitOptimizationResult visit_cond_branch(Argument<CondBranch> cond_branch) {
    return BitOptimizationResult::Unchanged;
  }
  BitOptimizationResult visit_branch(Argument<Branch> branch) {
    return BitOptimizationResult::Unchanged;
  }
  BitOptimizationResult visit_offset(Argument<Offset> offset) {
    return BitOptimizationResult::Unchanged;
  }
  BitOptimizationResult visit_call(Argument<Call> call) { return BitOptimizationResult::Unchanged; }
  BitOptimizationResult visit_stackalloc(Argument<StackAlloc> stackalloc) {
    return BitOptimizationResult::Unchanged;
  }
  BitOptimizationResult visit_ret(Argument<Ret> ret) { return BitOptimizationResult::Unchanged; }
};

bool opt::KnownBitsOptimization::run(Function* function) {
  bool did_something = false;

  // We need to traverse blocks in the DFS order.
  const auto blocks = function->entry_block()->reachable_blocks(TraversalType::DFS_WithStart);

  KnownBitsDatabase bits_database;

  for (Block* block : blocks) {
    for (Instruction& instruction : advance_early(*block)) {
      BitOptimizer optimizer(bits_database);
      const auto result = visitor::visit_instruction(&instruction, optimizer);

      switch (result) {
        case BitOptimizationResult::CalculatedBits:
        case BitOptimizationResult::ModifiedInstruction: {
          const auto bits = bits_database.get(&instruction);

          if (bits.mask == instruction.type()->bit_mask()) {
            instruction.replace_uses_with_constant_and_destroy(bits.value);
            did_something = true;
          }

          if (result == BitOptimizationResult::ModifiedInstruction) {
            did_something = true;
          }

          break;
        }

        case BitOptimizationResult::DestroyedInstruction:
          did_something = true;
          break;

        case BitOptimizationResult::Unchanged:
          break;
      }
    }
  }

  return did_something;
}