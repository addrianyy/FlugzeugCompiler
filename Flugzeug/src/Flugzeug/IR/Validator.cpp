#include "Validator.hpp"
#include "ConsoleIRPrinter.hpp"
#include "DominatorTree.hpp"
#include "Function.hpp"
#include "InstructionVisitor.hpp"

#include <Flugzeug/Core/ConsoleColors.hpp>

#include <fmt/format.h>
#include <iostream>

using namespace flugzeug;

#define validation_check(value, format, ...) \
  (check_fmt(__FILE__, __LINE__, !!(value), (format), ##__VA_ARGS__))

template <typename T>
class Format {
  const T* value;

 public:
  explicit Format(const T* value) : value(value) {}
  const T* get() const { return value; }
};

template <typename T>
struct fmt::formatter<Format<T>> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) const {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const Format<T>& value, FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "{}", value.get()->format());
  }
};

class Validator : public ConstInstructionVisitor {
  template <typename TInstruction, typename TVisitor>
  friend auto visitor::visit_instruction(TInstruction* instruction, TVisitor& visitor);

  const Function* function;
  DominatorTree dominator_tree;
  std::unordered_set<const Parameter*> parameters;
  std::unordered_set<const Block*> blocks;

  const Block* current_block = nullptr;
  const Instruction* current_instruction = nullptr;
  std::unordered_set<const Block*> current_block_predecessors;

  std::vector<ValidationResults::Error> errors;

  void add_error(const char* source_file, int source_line, std::string description) {
    errors.push_back(ValidationResults::Error{source_file, source_line, current_block,
                                              current_instruction, std::move(description)});
  }

  template <typename... Args>
  inline bool check_fmt(const char* file,
                        int line,
                        bool value,
                        fmt::format_string<Args...> fmt,
                        Args&&... args) {
    if (!value) {
      add_error(file, line, fmt::format(fmt, std::forward<Args>(args)...));
    }
    return value;
  }

  void visit_unary_instr(Argument<UnaryInstr> unary) {
    const auto type = unary->type();
    const auto val_type = unary->value()->type();

    validation_check(type == val_type,
                     "Unary instruction return type ({}) differs from operand type ({})",
                     Format(type), Format(val_type));
    validation_check(type->is_arithmetic(), "Unary instruction type is not arithmetic");
  }

  void visit_binary_instr(Argument<BinaryInstr> binary) {
    const auto type = binary->type();
    const auto lhs_type = binary->lhs()->type();
    const auto rhs_type = binary->rhs()->type();

    validation_check(lhs_type == rhs_type,
                     "Binary instruction LHS type ({}) differs from RHS type ({})",
                     Format(lhs_type), Format(rhs_type));
    validation_check(type == lhs_type,
                     "Binary instruction return type ({}) differs from operand type ({})",
                     Format(type), Format(lhs_type));
    validation_check(type->is_arithmetic(), "Binary instruction type is not arithmetic");
  }

  void visit_int_compare(Argument<IntCompare> int_compare) {
    const auto type = int_compare->type();
    const auto lhs_type = int_compare->lhs()->type();
    const auto rhs_type = int_compare->rhs()->type();

    validation_check(lhs_type == rhs_type,
                     "Compare instruction LHS type ({}) differs from RHS type ({})",
                     Format(lhs_type), Format(rhs_type));
    validation_check(lhs_type->is_arithmetic_or_pointer(),
                     "Compare instruction operands are not arithmetic or pointer types");
    validation_check(type->is_i1(), "Compare instruction doesn't return i1 but {}", Format(type));
  }

  void visit_load(Argument<Load> load) {
    const auto type = load->type();
    const auto address_type = load->address()->type();

    validation_check(address_type->is_pointer(), "Load operand isn't a pointer ({})",
                     Format(address_type));
    validation_check(type->ref() == address_type, "Load operand and return type mismatch");
    validation_check(type->is_arithmetic_or_pointer(),
                     "Loaded value isn't of arithmetic or pointer type ({})", Format(type));
  }

  void visit_store(Argument<Store> store) {
    const auto type = store->type();
    const auto address_type = store->address()->type();
    const auto value_type = store->value()->type();

    validation_check(value_type->ref() == address_type,
                     "Store destination and value type mismatch");
    validation_check(value_type->is_arithmetic_or_pointer(),
                     "Stored value isn't of arithmetic or pointer type ({})", Format(value_type));
    validation_check(type->is_void(), "Store doesn't return void ({})", Format(type));
  }

  void visit_branch(Argument<Branch> branch) {
    const auto type = branch->type();

    validation_check(type->is_void(), "Branch doesn't return void ({})", Format(type));
  }

  void visit_cond_branch(Argument<CondBranch> cond_branch) {
    const auto type = cond_branch->type();
    const auto cond_type = cond_branch->condition()->type();

    validation_check(cond_type->is_i1(), "Cond branch condition isn't i1 ({})", Format(cond_type));
    validation_check(type->is_void(), "Cond branch doesn't return void ({})", Format(type));
  }

  void visit_call(Argument<Call> call) {
    const auto type = call->type();
    const auto called_function = call->callee();

    validation_check(called_function->module() == function->module(),
                     "Call instruction crosses module boundary.");
    validation_check(type == called_function->return_type(),
                     "Call return type ({}) differs from functon return type", Format(type),
                     Format(called_function->return_type()));
    validation_check(called_function->parameter_count() == call->argument_count(),
                     "Call parameter count mismatch");

    for (size_t i = 0; i < called_function->parameter_count(); ++i) {
      const auto func_type = called_function->parameter(i)->type();
      const auto call_type = call->argument(i)->type();

      validation_check(call_type == func_type, "Call argument {}: expected {}, found {}", i,
                       Format(func_type), Format(call_type));
    }
  }

  void visit_stackalloc(Argument<StackAlloc> stackalloc) {
    const auto type = stackalloc->type();

    validation_check(stackalloc->size() > 0, "Stackalloc size is 0");
    if (validation_check(type->is_pointer(), "Stackalloc type isn't a pointer ({})",
                         Format(type))) {
      const auto allocated_type = stackalloc->allocated_type();
      validation_check(allocated_type->is_arithmetic_or_pointer(),
                       "Stackalloced type isn't arithmetic or pointer ({})",
                       Format(allocated_type));
    }
  }

  void visit_ret(Argument<Ret> ret) {
    const auto type = ret->type();
    const auto val_type = ret->return_value() ? ret->return_value()->type() : nullptr;

    const auto return_type = current_block->function()->return_type();
    if (return_type->is_void()) {
      validation_check(!ret->return_value(), "Void functions return non-void value");
    } else {
      validation_check(val_type == return_type, "Function returns {} but Ret operand is of type {}",
                       Format(return_type), Format(val_type));
    }

    validation_check(type->is_void(), "Ret doesn't return void ({})", Format(type));
  }

  void visit_offset(Argument<Offset> offset) {
    const auto type = offset->type();
    const auto base_type = offset->base()->type();
    const auto index_type = offset->index()->type();

    validation_check(type == base_type, "Offset base type ({}) and return type ({}) are mismatched",
                     Format(base_type), Format(type));
    validation_check(base_type->is_pointer(), "Base type isn't a pointer ({})", Format(base_type));
    validation_check(index_type->is_arithmetic(), "Index type isn't arithmetic ({})",
                     Format(index_type));
  }

  void visit_cast(Argument<Cast> cast) {
    const auto type = cast->type();
    const auto val_type = cast->casted_value()->type();
    const auto kind = cast->cast_kind();

    validation_check(val_type->is_arithmetic_or_pointer(),
                     "Cast operand ({}) is not arithmetic or pointer", Format(val_type));

    const auto from_bit_size = val_type->bit_size();
    const auto to_bit_size = type->bit_size();
    const auto both_arithmetic = val_type->is_arithmetic() && type->is_arithmetic();

    switch (kind) {
      case CastKind::Bitcast:
        validation_check(from_bit_size == to_bit_size, "Bitcast types must have the same size");
        break;

      case CastKind::Truncate:
        validation_check(both_arithmetic, "Truncate can only convert between arithmetic types");
        validation_check(from_bit_size > to_bit_size, "Truncate can only convert to smaller types");
        break;

      case CastKind::ZeroExtend:
      case CastKind::SignExtend:
        validation_check(both_arithmetic, "Sext/Zext can only convert between arithmetic types");
        validation_check(from_bit_size < to_bit_size, "Sext/Zext can only convert to bigger types");
        break;

      default:
        unreachable();
    }
  }

  void visit_select(Argument<Select> select) {
    const auto type = select->type();
    const auto cond_type = select->condition()->type();
    const auto true_type = select->true_value()->type();
    const auto false_type = select->false_value()->type();

    validation_check(cond_type->is_i1(), "Select condition isn't i1 ({})", Format(cond_type));
    validation_check(true_type == false_type,
                     "Select instruction true type ({}) differs from false type ({})",
                     Format(true_type), Format(false_type));
    validation_check(type == true_type,
                     "Select instruction return type ({}) differs from operand type ({})",
                     Format(type), Format(true_type));
    validation_check(type->is_arithmetic_or_pointer(),
                     "Select instruction type ({}) is not arithmetic or pointer", Format(type));
  }

  void visit_phi(Argument<Phi> phi) {
    const auto type = phi->type();
    validation_check(type->is_arithmetic_or_pointer() || type->is_i1(),
                     "Phi return type ({}) isn't arithmetic or pointer", Format(type));

    for (const auto incoming : *phi) {
      const auto value_type = incoming.value->type();
      validation_check(value_type == type,
                       "Phi incoming value `{}` ({}) has different type than Phi ({})",
                       Format(incoming.value), Format(value_type), Format(type));
    }
  }

  void check_instruction(const Instruction* instruction) {
    const auto phi = cast<Phi>(instruction);

    for (size_t i = 0; i < instruction->operand_count(); ++i) {
      const auto operand = instruction->operand(i);
      if (!validation_check(operand, "Instruction operand nr {} is null", i)) {
        continue;
      }

      validation_check(!operand->is_void(), "Instruction operand nr {} is void", i);
      validation_check(instruction->context() == operand->context(),
                       "Instruction operand `{}` has mismatched context", Format(operand));

      if (const auto parameter = cast<Parameter>(operand)) {
        validation_check(parameters.contains(parameter),
                         "Instruction uses parameter outside of the function");
      } else if (const auto block = cast<Block>(operand)) {
        validation_check(blocks.contains(block), "Instruction uses block outside of the function");
      } else if (const auto other = cast<Instruction>(operand)) {
        const auto is_inserted = other->block();
        validation_check(is_inserted, "Using uninserted instruction as operand");

        if (!phi) {
          validation_check(instruction != other,
                           "Self references are only allowed in Phi instructions");

          if (is_inserted) {
            validation_check(other->dominates(instruction, dominator_tree),
                             "`{}` doesn't dominate this instruction", Format(other));
          }
        }

        if (const auto other_phi = cast<Phi>(other)) {
          validation_check(!other_phi->empty(), "Instruction used empty Phi (`{}`) as an operand",
                           Format(other));
        }
      }
    }

    if (phi) {
      const auto incoming_count = phi->incoming_count();
      validation_check(incoming_count == current_block_predecessors.size(),
                       "Phi incoming blocks and block predecessors are mismatched");

      for (const auto incoming : *phi) {
        validation_check(current_block_predecessors.contains(incoming.block),
                         "Phi has incoming block `{}` which isn't a predecessor",
                         Format(incoming.block));

        // If this incoming block is dead skip dominance checks.
        if (dominator_tree.is_block_dead(incoming.block)) {
          continue;
        }

        if (const auto incoming_instruction = cast<Instruction>(incoming.value)) {
          if (incoming_instruction->block()) {
            // Simulate usage at the end of predecessor.
            validation_check(
              incoming_instruction->dominates(incoming.block->last_instruction(), dominator_tree),
              "Phi has incoming value `{}` which doesn't dominate the last instruction of `{}`",
              Format(incoming.value), Format(incoming.block));
          }
        }
      }
    }

    // Do per-instruction operand type checking.
    visitor::visit_instruction(instruction, *this);
  }

 public:
  explicit Validator(const Function* function) : function(function), dominator_tree(function) {
    for (size_t i = 0; i < function->parameter_count(); ++i) {
      parameters.insert(function->parameter(i));
    }

    for (const Block& block : *function) {
      blocks.insert(&block);
    }
  }

  ValidationResults validate() {
    for (const Block& block : *function) {
      current_block = &block;

      // Dead blocks should be ignored as they are allowed to be invalid.
      if (dominator_tree.is_block_dead(&block)) {
        continue;
      }

      current_block_predecessors = block.predecessors_set();
      if (block.is_entry_block()) {
        validation_check(current_block_predecessors.empty(), "Entry block has predecessors");
      }

      // Empty blocks are invalid.
      if (!validation_check(!block.empty(), "Block is empty")) {
        continue;
      }

      for (const Instruction& instruction : block) {
        current_instruction = &instruction;

        validation_check(instruction.context() == function->context(),
                         "Instruction in block has mismatched context");

        check_instruction(&instruction);

        if (block.last_instruction() == &instruction) {
          validation_check(instruction.is_terminator(), "Block doesn't end in terminator");
        } else {
          validation_check(!instruction.is_terminator(),
                           "Terminator is in the middle of the block");
        }

        current_instruction = nullptr;
      }

      current_block = nullptr;
    }

    return ValidationResults(std::move(errors));
  }
};

static bool use_colors(std::ostream& stream) {
  return &stream == &std::cout && ConsoleColors::supported();
}

static void reset_console_color(std::ostream& stream) {
  if (use_colors(stream)) {
    ConsoleColors::reset_color(stream);
  }
}

static void set_console_color(std::ostream& stream, int color) {
  if (use_colors(stream)) {
    ConsoleColors::set_color(stream, color);
  }
}

static void print_error(const Function* function,
                        std::ostream& stream,
                        ConsoleIRPrinter& printer,
                        const ValidationResults::Error& error) {
  // Red
  const int key_color = 31;
  const int header_color = 31;

  set_console_color(stream, header_color);
  stream << "Validation error at " << error.source_file << ":" << error.source_line << std::endl;
  reset_console_color(stream);

  set_console_color(stream, key_color);
  stream << "  Function:    ";
  reset_console_color(stream);
  stream << function->name() << std::endl;

  if (error.block) {
    set_console_color(stream, key_color);
    stream << "  Block:       ";
    reset_console_color(stream);
    stream << error.block->format() << std::endl;
  }

  if (error.instruction) {
    set_console_color(stream, key_color);
    stream << "  Instruction: ";
    reset_console_color(stream);
    error.instruction->print(printer);
  }

  set_console_color(stream, key_color);
  stream << "  Message:     ";
  reset_console_color(stream);
  stream << error.description << std::endl;
}

namespace flugzeug {

ValidationResults validate_function(const Function* function, ValidationBehaviour behaviour) {
  ConsoleColors::ensure_initialized();

  Validator validator(function);
  auto results = validator.validate();

  if (behaviour != ValidationBehaviour::Silent) {
    std::ostream& stream = std::cout;
    ConsoleIRPrinter printer(ConsoleIRPrinter::Variant::ColorfulIfSupported, stream);

    for (const auto& error : results) {
      print_error(function, stream, printer, error);
      stream << std::endl;
    }

    stream.flush();

    if (behaviour == ValidationBehaviour::ErrorsAreFatal && results.has_errors()) {
      const auto error_count = results.errors().size();
      if (error_count == 1) {
        fatal_error("Encountered 1 validation error in function {}.", function->name());
      } else {
        fatal_error("Encountered {} validation errors in function {}.", error_count,
                    function->name());
      }
    }
  }

  return results;
}

}  // namespace flugzeug