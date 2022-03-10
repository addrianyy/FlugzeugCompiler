#pragma once
#include "Token.hpp"

namespace flugzeug {

struct PRType {
  Token::Keyword base_kw;
  uint32_t indirection;
};

struct PRInstructionOperand {
  enum class Kind {
    Value,
    Constant,
    Undef,

    Block,
    Function,
  };

  Kind kind;
  PRType type{};
  std::string_view name;
  uint64_t constant;
};

struct PRInstruction {
  std::string_view result_value;

  Token::Keyword kind_kw;
  std::vector<PRInstructionOperand> operands;

  // Instruction-specific data.
  PRType specific_type{};
  size_t specific_size{};
  Token::Keyword specific_keyword;

  void add_operand(PRInstructionOperand operand) { operands.push_back(operand); }
};

struct PRFunctionBlock {
  std::string_view name;
  std::vector<std::unique_ptr<PRInstruction>> instructions;

  explicit PRFunctionBlock(std::string_view name) : name(name) {}
};

struct PRFunctionParameter {
  PRType type;
  std::string_view name;
};

struct PRFunction {
  bool is_extern;
  PRType return_type;
  std::string_view name;
  std::vector<PRFunctionParameter> parameters;
  std::vector<std::unique_ptr<PRFunctionBlock>> blocks;

  PRFunction(bool is_extern, const PRType& return_type, const std::string_view& name,
             std::vector<PRFunctionParameter> parameters)
      : is_extern(is_extern), return_type(return_type), name(name),
        parameters(std::move(parameters)) {}
};

} // namespace flugzeug