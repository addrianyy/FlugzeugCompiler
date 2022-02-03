#include "Validator.hpp"
#include "ConsolePrinter.hpp"
#include "DominatorTree.hpp"
#include "Function.hpp"
#include "InstructionVisitor.hpp"

#include <Flugzeug/Core/ConsoleColors.hpp>

#include <fmt/format.h>
#include <iostream>

using namespace flugzeug;

#define validation_check(value, format, ...)                                                       \
  (check_fmt(__FILE__, __LINE__, !!(value), (format), ##__VA_ARGS__))

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

  template <typename S, typename... Args>
  inline bool check_fmt(const char* file, int line, bool value, const S& format, Args&&... args) {
    if (!value) {
      add_error(file, line, fmt::format(format, args...));
    }
    return value;
  }

  void visit_unary_instr(Argument<UnaryInstr> unary) {
    const auto type = unary->get_type();
    const auto val_type = unary->get_val()->get_type();

    validation_check(type == val_type,
                     "Unary instruction return type ({}) differs from operand type ({})",
                     type->format(), val_type->format());
    validation_check(type->is_arithmetic(), "Unary instruction type is not arithmetic");
  }

  void visit_binary_instr(Argument<BinaryInstr> binary) {
    const auto type = binary->get_type();
    const auto lhs_type = binary->get_lhs()->get_type();
    const auto rhs_type = binary->get_rhs()->get_type();

    validation_check(lhs_type == rhs_type,
                     "Binary instruction LHS type ({}) differs from RHS type ({})",
                     lhs_type->format(), rhs_type->format());
    validation_check(type == lhs_type,
                     "Binary instruction return type ({}) differs from operand type ({})",
                     type->format(), lhs_type->format());
    validation_check(type->is_arithmetic(), "Binary instruction type is not arithmetic");
  }

  void visit_int_compare(Argument<IntCompare> int_compare) {
    const auto type = int_compare->get_type();
    const auto lhs_type = int_compare->get_lhs()->get_type();
    const auto rhs_type = int_compare->get_rhs()->get_type();

    validation_check(lhs_type == rhs_type,
                     "Compare instruction LHS type ({}) differs from RHS type ({})",
                     lhs_type->format(), rhs_type->format());
    validation_check(lhs_type->is_arithmetic_or_pointer(),
                     "Compare instruction operands are not arithmetic or pointer types");
    validation_check(type->is_i1(), "Compare instruction doesn't return i1 but {}", type->format());
  }

  void visit_load(Argument<Load> load) {
    const auto type = load->get_type();
    const auto ptr_type = load->get_ptr()->get_type();

    validation_check(ptr_type->is_pointer(), "Load operand isn't a pointer ({})",
                     ptr_type->format());
    validation_check(type->ref() == ptr_type, "Load operand and return type mismatch");
    validation_check(type->is_arithmetic_or_pointer(),
                     "Loaded value isn't of arithmetic or pointer type ({})", type->format());
  }

  void visit_store(Argument<Store> store) {
    const auto type = store->get_type();
    const auto ptr_type = store->get_ptr()->get_type();
    const auto val_type = store->get_val()->get_type();

    validation_check(val_type->ref() == ptr_type, "Store destination and value type mismatch");
    validation_check(val_type->is_arithmetic_or_pointer(),
                     "Stored value isn't of arithmetic or pointer type ({})", val_type->format());
    validation_check(type->is_void(), "Store doesn't return void ({})", type->format());
  }

  void visit_branch(Argument<Branch> branch) {
    const auto type = branch->get_type();

    validation_check(type->is_void(), "Branch doesn't return void ({})", type->format());
  }

  void visit_cond_branch(Argument<CondBranch> cond_branch) {
    const auto type = cond_branch->get_type();
    const auto cond_type = cond_branch->get_cond()->get_type();

    validation_check(cond_type->is_i1(), "Cond branch condition isn't i1 ({})",
                     cond_type->format());
    validation_check(type->is_void(), "Cond branch doesn't return void ({})", type->format());
  }

  void visit_call(Argument<Call> call) {
    const auto type = call->get_type();
    const auto called_function = call->get_callee();

    validation_check(type == called_function->get_return_type(),
                     "Call return type ({}) differs from functon return type", type->format(),
                     called_function->get_return_type()->format());
    validation_check(called_function->get_parameter_count() == call->get_arg_count(),
                     "Call parameter count mismatch");

    for (size_t i = 0; i < called_function->get_parameter_count(); ++i) {
      const auto func_type = called_function->get_parameter(i)->get_type();
      const auto call_type = call->get_arg(i)->get_type();

      validation_check(call_type == func_type, "Call argument {}: expected {}, found {}", i,
                       func_type->format(), call_type->format());
    }
  }

  void visit_stackalloc(Argument<StackAlloc> stackalloc) {
    const auto type = stackalloc->get_type();

    validation_check(stackalloc->get_size() > 0, "Stackalloc size is 0");
    if (validation_check(type->is_pointer(), "Stackalloc type isn't a pointer ({})",
                         type->format())) {
      const auto allocated_type = stackalloc->get_allocated_type();
      validation_check(allocated_type->is_arithmetic_or_pointer(),
                       "Stackalloced type isn't arithmetic or pointer ({})",
                       allocated_type->format());
    }
  }

  void visit_ret(Argument<Ret> ret) {
    const auto type = ret->get_type();
    const auto val_type = ret->get_val() ? ret->get_val()->get_type() : nullptr;

    const auto return_type = current_block->get_function()->get_return_type();
    if (return_type->is_void()) {
      validation_check(!ret->get_val(), "Void functions return non-void value");
    } else {
      validation_check(val_type == return_type, "Function returns {} but Ret operand is of type {}",
                       return_type->format(), val_type->format());
    }

    validation_check(type->is_void(), "Ret doesn't return void ({})", type->format());
  }

  void visit_offset(Argument<Offset> offset) {
    const auto type = offset->get_type();
    const auto base_type = offset->get_base()->get_type();
    const auto index_type = offset->get_index()->get_type();

    validation_check(type == base_type, "Offset base type ({}) and return type ({}) are mismatched",
                     base_type->format(), type->format());
    validation_check(base_type->is_pointer(), "Base type isn't a pointer ({})",
                     base_type->format());
    validation_check(index_type->is_arithmetic(), "Index type isn't arithmetic ({})",
                     index_type->format());
  }

  void visit_cast(Argument<Cast> cast) {
    const auto type = cast->get_type();
    const auto val_type = cast->get_val()->get_type();
    const auto kind = cast->get_cast_kind();

    validation_check(val_type->is_arithmetic_or_pointer(),
                     "Cast operand ({}) is not arithmetic or pointer", val_type->format());

    const auto from_bit_size = val_type->get_bit_size();
    const auto to_bit_size = type->get_bit_size();
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
    const auto type = select->get_type();
    const auto cond_type = select->get_cond()->get_type();
    const auto true_type = select->get_true_val()->get_type();
    const auto false_type = select->get_false_val()->get_type();

    validation_check(cond_type->is_i1(), "Select condition isn't i1 ({})", cond_type->format());
    validation_check(true_type == false_type,
                     "Select instruction true type ({}) differs from false type ({})",
                     true_type->format(), false_type->format());
    validation_check(type == true_type,
                     "Select instruction return type ({}) differs from operand type ({})",
                     type->format(), true_type->format());
    validation_check(type->is_arithmetic_or_pointer(),
                     "Select instruction type ({}) is not arithmetic or pointer", type->format());
  }

  void visit_phi(Argument<Phi> phi) {
    const auto type = phi->get_type();
    validation_check(type->is_arithmetic_or_pointer(),
                     "Phi return type ({}) isn't arithmetic or pointer", type->format());

    for (const auto incoming : *phi) {
      const auto value_type = incoming.value->get_type();
      validation_check(value_type == type,
                       "Phi incoming value `{}` ({}) has different type than Phi ({})",
                       incoming.value->format(), value_type->format(), type->format());
    }
  }

  void check_instruction(const Instruction* instruction) {
    const auto phi = cast<Phi>(instruction);

    for (size_t i = 0; i < instruction->get_operand_count(); ++i) {
      const auto operand = instruction->get_operand(i);
      if (!validation_check(operand, "Instruction operand nr {} is null", i)) {
        continue;
      }

      validation_check(!operand->is_void(), "Instruction operand nr {} is void", i);
      validation_check(instruction->get_context() == operand->get_context(),
                       "Instruction operand `{}` has mismatched context", operand->format());

      if (const auto parameter = cast<Parameter>(operand)) {
        validation_check(parameters.contains(parameter),
                         "Instruction uses parameter outside of the function");
      } else if (const auto block = cast<Block>(operand)) {
        validation_check(blocks.contains(block), "Instruction uses block outside of the function");
      } else if (const auto other = cast<Instruction>(operand)) {
        verify(other->get_block(), "Using uninserted instruction as operand");

        if (!phi) {
          validation_check(instruction != other,
                           "Self references are only allowed in Phi instructions");
          validation_check(other->dominates(instruction, dominator_tree),
                           "`{}` doesn't dominate this instruction", other->format());
        }

        if (const auto other_phi = cast<Phi>(other)) {
          validation_check(!other_phi->is_empty(),
                           "Instruction used empty Phi (`{}`) as an operand", other->format());
        }
      }
    }

    if (phi) {
      const auto incoming_count = phi->get_incoming_count();
      validation_check(incoming_count == current_block_predecessors.size(),
                       "Phi incoming blocks and block predecessors are mismatched");

      for (const auto incoming : *phi) {
        validation_check(current_block_predecessors.contains(incoming.block),
                         "Phi has incoming block `{}` which isn't a predecessor",
                         incoming.block->format());

        // If this incoming block is dead skip dominance checks.
        if (dominator_tree.is_block_dead(incoming.block)) {
          continue;
        }

        if (const auto incoming_instruction = cast<Instruction>(incoming.value)) {
          // Simulate usage at the end of predecessor.
          validation_check(
            incoming_instruction->dominates(incoming.block->get_last_instruction(), dominator_tree),
            "Phi has incoming value `{}` which doesn't dominate the last instruction of `{}`",
            incoming.value->format(), incoming.block->format());
        }
      }
    }

    // Do per-instruction operand type checking.
    visitor::visit_instruction(instruction, *this);
  }

public:
  explicit Validator(const Function* function) : function(function), dominator_tree(function) {
    for (size_t i = 0; i < function->get_parameter_count(); ++i) {
      parameters.insert(function->get_parameter(i));
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

      current_block_predecessors = block.get_predecessors();
      if (block.is_entry_block()) {
        validation_check(current_block_predecessors.empty(), "Entry block has predecessors");
      }

      // Empty blocks are invalid.
      if (!validation_check(!block.is_empty(), "Block is empty")) {
        continue;
      }

      for (const Instruction& instruction : block) {
        current_instruction = &instruction;

        validation_check(instruction.get_context() == function->get_context(),
                         "Instruction in block has mismatched context");

        check_instruction(&instruction);

        if (block.get_last_instruction() == &instruction) {
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
  return &stream == &std::cout && console_colors::are_allowed();
}

static void reset_console_color(std::ostream& stream) {
  if (use_colors(stream)) {
    console_colors::reset_color(stream);
  }
}

static void set_console_color(std::ostream& stream, int color) {
  if (use_colors(stream)) {
    console_colors::set_color(stream, color);
  }
}

static void print_error(const Function* function, std::ostream& stream, ConsolePrinter& printer,
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
  stream << function->get_name() << std::endl;

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
  console_colors::ensure_initialized();

  Validator validator(function);
  auto results = validator.validate();

  if (behaviour != ValidationBehaviour::Silent) {
    std::ostream& stream = std::cout;
    ConsolePrinter printer(ConsolePrinter::Variant::ColorfulIfSupported, stream);

    for (const auto& error : results) {
      print_error(function, stream, printer, error);
      stream << std::endl;
    }

    stream.flush();

    if (behaviour == ValidationBehaviour::ErrorsAreFatal && results.has_errors()) {
      const auto error_count = results.get_errors().size();
      if (error_count == 1) {
        fatal_error("Encountered 1 validation error in function {}.", function->get_name());
      } else {
        fatal_error("Encountered {} validation errors in function {}.", error_count,
                    function->get_name());
      }
    }
  }

  return results;
}

} // namespace flugzeug