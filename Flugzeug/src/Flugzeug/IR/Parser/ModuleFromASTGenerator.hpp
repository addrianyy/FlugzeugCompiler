#pragma once
#include "AST.hpp"

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/IR/Module.hpp>

namespace flugzeug {

class ModuleFromASTGenerator {
  struct FunctionGenerationContext {
    const PRFunction* pr_function;
    Function* function;

    std::unordered_map<std::string_view, Block*> ir_block_map;
    std::unordered_map<std::string_view, Value*> ir_value_map;
    std::unordered_map<const PRInstruction*, Instruction*> ir_instruction_map;
    std::unordered_map<std::string_view, const PRInstruction*> name_to_instruction_map;
  };

  Context* context;
  Module* module;
  FunctionGenerationContext fn_ctx{};

  std::vector<std::unique_ptr<PRFunction>> parsed_functions;

  const PRInstruction* get_instruction_from_name(std::string_view name);

  Type* convert_type(PRType type);

  Block* operand_to_block(const PRInstruction* instruction, size_t operand_index);
  Block* operand_to_block(const PRInstructionOperand& operand);

  Value* operand_to_value(const PRInstruction* instruction, size_t operand_index);
  Value* operand_to_value(const PRInstructionOperand& operand);

  Instruction* generate_instruction(const PRInstruction* instruction);

  void generate_function_body();

public:
  ModuleFromASTGenerator(Module* module, std::vector<std::unique_ptr<PRFunction>> parsed_functions);

  void generate();
};

} // namespace flugzeug