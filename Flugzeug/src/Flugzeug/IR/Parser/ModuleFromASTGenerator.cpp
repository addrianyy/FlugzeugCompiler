#include "ModuleFromASTGenerator.hpp"

#include <deque>

using namespace flugzeug;

template <typename Map, typename K, typename V>
void insert_no_duplicates(Map& map, const K& key, const V& value) {
  verify(map.insert({key, value}).second, "{} is already defined", key);
}

const PRInstruction* ModuleFromASTGenerator::get_instruction_from_name(std::string_view name) {
  const auto it = fn_ctx.name_to_instruction_map.find(name);
  if (it == fn_ctx.name_to_instruction_map.end()) {
    return nullptr;
  }

  return it->second;
}

Type* ModuleFromASTGenerator::convert_type(PRType type) {
  Type* result = nullptr;

  // clang-format off
  switch (type.base_kw) {
    case Token::Keyword::I1: result = context->i1_ty(); break;
    case Token::Keyword::I8: result = context->i8_ty(); break;
    case Token::Keyword::I16: result = context->i16_ty(); break;
    case Token::Keyword::I32: result = context->i32_ty(); break;
    case Token::Keyword::I64: result = context->i64_ty(); break;
    case Token::Keyword::Void: result = context->void_ty(); break;
    default: unreachable();
  }
  // clang-format on

  if (type.indirection > 0) {
    result = result->ref(type.indirection);
  }

  return result;
}

Block* ModuleFromASTGenerator::operand_to_block(const PRInstruction* instruction,
                                                size_t operand_index) {
  return operand_to_block(instruction->get_operand(operand_index));
}

Block* ModuleFromASTGenerator::operand_to_block(const PRInstructionOperand& operand) {
  verify(operand.kind == PRInstructionOperand::Kind::Block, "Expected block instruction operand");

  const auto it = fn_ctx.ir_block_map.find(operand.name);
  verify(it != fn_ctx.ir_block_map.end(), "Undefined block `{}` was used", operand.name);

  return it->second;
}

Value* ModuleFromASTGenerator::operand_to_value(const PRInstruction* instruction,
                                                size_t operand_index) {
  return operand_to_value(instruction->get_operand(operand_index));
}

Value* ModuleFromASTGenerator::operand_to_value(const PRInstructionOperand& operand) {
  Type* type = convert_type(operand.type);
  Value* result = nullptr;

  switch (operand.kind) {
    case PRInstructionOperand::Kind::Value: {
      const auto it = fn_ctx.ir_value_map.find(operand.name);
      verify(it != fn_ctx.ir_value_map.end(), "Undefined value `{}` was used", operand.name);

      result = it->second;
      break;
    }

    case PRInstructionOperand::Kind::Constant: {
      result = type->constant(operand.constant);
      break;
    }

    case PRInstructionOperand::Kind::Undef: {
      result = type->undef();
      break;
    }

    default:
      unreachable();
  }

  verify(result, "Failed to get value from instruction operand");

  if (operand.kind == PRInstructionOperand::Kind::Value && result->type() != type) {
    fatal_error("{}: Type mismatch for `{}`. Expected `{}`, found `{}`.", fn_ctx.pr_function->name,
                operand.name, type->format(), result->type()->format());
  }

  return result;
}

Instruction* ModuleFromASTGenerator::generate_instruction(const PRInstruction* instruction) {
  const auto kind_kw = instruction->kind_kw;

  if (const auto unary_op = Token::keyword_to_unary_op(kind_kw)) {
    return new UnaryInstr(context, *unary_op, operand_to_value(instruction, 0));
  }

  if (const auto binary_op = Token::keyword_to_binary_op(kind_kw)) {
    return new BinaryInstr(context, operand_to_value(instruction, 0), *binary_op,
                           operand_to_value(instruction, 1));
  }

  if (const auto cast_kind = Token::keyword_to_cast(kind_kw)) {
    return new Cast(context, *cast_kind, operand_to_value(instruction, 0),
                    convert_type(instruction->specific_type));
  }

  switch (kind_kw) {
    case Token::Keyword::Cmp: {
      const auto predicate = Token::keyword_to_int_predicate(instruction->specific_keyword);
      verify(predicate, "Invalid keyword for int predicate");
      return new IntCompare(context, operand_to_value(instruction, 0), *predicate,
                            operand_to_value(instruction, 1));
    }

    case Token::Keyword::Load: {
      const auto load = new Load(context, operand_to_value(instruction, 0));
      const auto expected_type = convert_type(instruction->specific_type);
      if (load->type() != expected_type) {
        fatal_error("{}: Type mismatch for load result. Expected `{}`, found `{}`.",
                    fn_ctx.pr_function->name, expected_type->format(), load->type()->format());
      }
      return load;
    }

    case Token::Keyword::Store: {
      return new Store(context, operand_to_value(instruction, 0), operand_to_value(instruction, 1));
    }

    case Token::Keyword::Call: {
      const auto return_type = convert_type(instruction->specific_type);
      const auto callee_name = instruction->specific_name;

      const auto callee = module->find_function(callee_name);
      verify(callee, "Undefined function `{}` called", callee_name);

      if (return_type != callee->return_type()) {
        fatal_error("{}: Type mismatch for `{}` call result. Expected `{}`, found `{}`.",
                    fn_ctx.pr_function->name, callee->name(), return_type->format(),
                    callee->return_type()->format());
      }

      std::vector<Value*> arguments;
      arguments.reserve(instruction->operands.size());

      for (const auto& operand : instruction->operands) {
        arguments.push_back(operand_to_value(operand));
      }

      return new Call(context, callee, arguments);
    }

    case Token::Keyword::Branch: {
      return new Branch(context, operand_to_block(instruction, 0));
    }

    case Token::Keyword::Bcond: {
      return new CondBranch(context, operand_to_value(instruction, 0),
                            operand_to_block(instruction, 1), operand_to_block(instruction, 2));
    }

    case Token::Keyword::Stackalloc: {
      return new StackAlloc(context, convert_type(instruction->specific_type),
                            instruction->specific_size);
    }

    case Token::Keyword::Ret: {
      if (instruction->operands.empty()) {
        return new Ret(context);
      } else {
        return new Ret(context, operand_to_value(instruction, 0));
      }
    }

    case Token::Keyword::Offset: {
      return new Offset(context, operand_to_value(instruction, 0),
                        operand_to_value(instruction, 1));
    }

    case Token::Keyword::Select: {
      return new Select(context, operand_to_value(instruction, 0), operand_to_value(instruction, 1),
                        operand_to_value(instruction, 2));
    }

    case Token::Keyword::Phi: {
      // We will fill the incoming values later.
      return new Phi(context, convert_type(instruction->specific_type));
    }

    default:
      unreachable();
  }
}

void ModuleFromASTGenerator::generate_function_body() {
  // Setup mapping from parameter name to parameter IR value.
  {
    size_t index = 0;
    for (const auto& parameter : fn_ctx.pr_function->parameters) {
      insert_no_duplicates(fn_ctx.ir_value_map, parameter.name,
                           fn_ctx.function->parameter(index));

      index++;
    }
  }

  // Create all function blocks and setup block and instruction mappings.
  for (const auto& pr_block : fn_ctx.pr_function->blocks) {
    const auto block = fn_ctx.function->create_block();
    insert_no_duplicates(fn_ctx.ir_block_map, pr_block->name, block);

    for (const auto& pr_instruction : pr_block->instructions) {
      if (!pr_instruction->result_value.empty()) {
        insert_no_duplicates(fn_ctx.name_to_instruction_map, pr_instruction->result_value,
                             pr_instruction.get());
      }
    }
  }

  // At this point instructions and their operands should create a DAG (if we skip Phi incoming
  // values). We will iterate over them in order that ensures that all instruction operands were
  // already visited before.

  // Map Instruction -> Instruction that use this instruction.
  std::unordered_map<const PRInstruction*, std::unordered_set<const PRInstruction*>> users;

  std::deque<const PRInstruction*> instruction_queue;

  // Go through every instruction in the function. Add instructions which don't depend on other
  // instructions to the queue. Also create users map.
  for (auto& pr_block : fn_ctx.pr_function->blocks) {
    for (auto& pr_instruction : pr_block->instructions) {
      bool no_instruction_dependencies = true;

      // Ignore Phi incoming values, we will handle them later.
      if (pr_instruction->kind_kw != Token::Keyword::Phi) {
        for (const auto& operand : pr_instruction->operands) {
          if (operand.kind != PRInstructionOperand::Kind::Value) {
            continue;
          }

          const auto instruction_operand = get_instruction_from_name(operand.name);
          if (!instruction_operand) {
            continue;
          }

          // This instruction depends on another instruction. Add it as a user of
          // `instruction_operand`.
          users[instruction_operand].insert(pr_instruction.get());
          no_instruction_dependencies = false;
        }
      }

      if (no_instruction_dependencies) {
        instruction_queue.push_back(pr_instruction.get());
      }
    }
  }

  while (!instruction_queue.empty()) {
    // Get the instruction from the front of the queue.
    const auto pr_instruction = instruction_queue.front();
    instruction_queue.pop_front();

    // Generate IR instruction.
    const auto instruction = generate_instruction(pr_instruction);
    verify(instruction, "Failed to generate IR instruction");
    verify(fn_ctx.ir_instruction_map.insert({pr_instruction, instruction}).second,
           "Instruction already generated (?)");

    // This instruction created a value. Add it to value map as it may be used as some other
    // instruction's operand.
    if (!pr_instruction->result_value.empty()) {
      insert_no_duplicates(fn_ctx.ir_value_map, pr_instruction->result_value, instruction);
    }

    // As we have created this instruction we may now have instructions that have all their
    // operands handled. Find them and add them to the queue.

    const auto users_it = users.find(pr_instruction);
    if (users_it == users.end()) {
      continue;
    }

    for (const auto user : users_it->second) {
      const bool all_operands_handled =
        all_of(user->operands, [&](const PRInstructionOperand& operand) {
          if (operand.kind != PRInstructionOperand::Kind::Value) {
            return true;
          }

          const auto instruction_operand = get_instruction_from_name(operand.name);
          if (!instruction_operand) {
            return true;
          }

          // This instruction depends on another instruction. Check if that instruction has been
          // already handled.
          return fn_ctx.ir_instruction_map.contains(instruction_operand);
        });

      if (all_operands_handled) {
        instruction_queue.push_back(user);
      }
    }
  }

  // At this point we have created all instructions unless there are cycles. Now insert them in
  // correct places and add Phi incoming values.
  for (const auto& pr_block : fn_ctx.pr_function->blocks) {
    const auto block = fn_ctx.ir_block_map[pr_block->name];

    for (const auto& pr_instruction : pr_block->instructions) {
      // Get the IR instruction.
      const auto instruction_it = fn_ctx.ir_instruction_map.find(pr_instruction.get());
      verify(instruction_it != fn_ctx.ir_instruction_map.end(),
             "{}: Couldn't generate IR - cycles were found in the instructions",
             fn_ctx.pr_function->name);
      const auto instruction = instruction_it->second;

      // Insert instruction in the correct place.
      block->push_instruction_back(instruction_it->second);

      // Add incoming values to Phi instructions.
      if (const auto phi = cast<Phi>(instruction)) {
        verify(pr_instruction->operands.size() % 2 == 0, "Phi operand count is not even");

        for (size_t incoming = 0; incoming < pr_instruction->operands.size() / 2; ++incoming) {
          const auto i_block = operand_to_block(pr_instruction.get(), incoming * 2 + 0);
          const auto i_value = operand_to_value(pr_instruction.get(), incoming * 2 + 1);
          phi->add_incoming(i_block, i_value);
        }
      }
    }
  }
}

ModuleFromASTGenerator::ModuleFromASTGenerator(
  Module* module,
  std::vector<std::unique_ptr<PRFunction>> parsed_functions)
    : context(module->context()), module(module), parsed_functions(std::move(parsed_functions)) {
  verify(module->empty(), "Module passed to ModuleFromASTGenerator must be empty");
}

void ModuleFromASTGenerator::generate() {
  for (const auto& pr_function : parsed_functions) {
    std::vector<Type*> parameter_types;
    parameter_types.reserve(pr_function->parameters.size());

    for (const auto& parameter : pr_function->parameters) {
      parameter_types.push_back(convert_type(parameter.type));
    }

    module->create_function(convert_type(pr_function->return_type), std::string(pr_function->name),
                            parameter_types);
  }

  for (const auto& pr_function : parsed_functions) {
    if (pr_function->is_extern) {
      continue;
    }

    verify(!pr_function->blocks.empty(), "Non-extern functions must contain blocks");

    const auto function = module->find_function(pr_function->name);
    verify(function, "Failed to get the IR function");

    fn_ctx = FunctionGenerationContext{pr_function.get(), function};
    generate_function_body();
    fn_ctx = FunctionGenerationContext{};
  }
}