#include "Parser.hpp"

using namespace flugzeug;

static bool is_type_keyword(Token::Keyword keyword) {
  switch (keyword) {
    case Token::Keyword::Void:
    case Token::Keyword::I1:
    case Token::Keyword::I8:
    case Token::Keyword::I16:
    case Token::Keyword::I32:
    case Token::Keyword::I64:
      return true;
    default:
      return false;
  }
}

void Parser::skip_newlines() {
  while (lexer.current_token().is(Token::Kind::NewLine)) {
    lexer.consume_token();
  }
}

PRType Parser::parse_type() {
  const auto type_kw = lexer.consume_keyword();
  if (!is_type_keyword(type_kw)) {
    fatal_error("Expected type keyword, found {}.", Token::stringify_keyword(type_kw));
  }

  uint32_t indirection = 0;
  while (lexer.current_token().is(Token::Kind::Star)) {
    lexer.consume_token();
    indirection++;
  }

  return PRType{type_kw, indirection};
}

PRInstructionOperand Parser::parse_value_operand_with_type(PRType type) {
  PRInstructionOperand operand{};
  operand.type = type;

  const auto token = lexer.consume_token();

  if (token.is_identifier()) {
    operand.kind = PRInstructionOperand::Kind::Value;
    operand.name = token.get_identifier();

    return operand;
  }

  if (token.is_literal()) {
    operand.kind = PRInstructionOperand::Kind::Constant;
    operand.constant = token.get_literal();

    return operand;
  }

  if (token.is_keyword()) {
    const auto keyword = token.get_keyword();

    if (keyword == Token::Keyword::Undef) {
      operand.kind = PRInstructionOperand::Kind::Undef;

      return operand;
    }

    if (keyword == Token::Keyword::Null) {
      verify(operand.type.indirection > 0, "Null constant can be only used for pointer values");

      operand.kind = PRInstructionOperand::Kind::Constant;
      operand.constant = 0;

      return operand;
    }

    if (keyword == Token::Keyword::True || keyword == Token::Keyword::False) {
      verify(operand.type.base_kw == Token::Keyword::I1 && operand.type.indirection == 0,
             "True/false constants can be only used for i1 values");

      operand.kind = PRInstructionOperand::Kind::Constant;
      operand.constant = keyword == Token::Keyword::True ? 1 : 0;

      return operand;
    }
  }

  fatal_error("Failed to parse instruction operand: {}", token.format());
}

PRInstructionOperand Parser::parse_value_operand() {
  return parse_value_operand_with_type(parse_type());
}

PRInstructionOperand Parser::parse_block_operand() {
  PRInstructionOperand operand{};
  operand.kind = PRInstructionOperand::Kind::Block;
  operand.name = lexer.consume_identifier();

  return operand;
}

void Parser::parse_call_instruction(PRInstruction* instruction) {
  instruction->specific_type = parse_type();
  instruction->specific_name = lexer.consume_identifier();

  parse_argument_list(Token::Kind::ParenOpen, Token::Kind::ParenClose,
                      [&]() { instruction->add_operand(parse_value_operand()); });
}

std::unique_ptr<PRInstruction> Parser::parse_void_instruction() {
  const auto keyword = lexer.consume_keyword();

  auto instruction = std::make_unique<PRInstruction>();
  instruction->kind_kw = keyword;

  switch (keyword) {
    case Token::Keyword::Store: {
      instruction->add_operand(parse_value_operand());
      lexer.consume_expect(Token::Kind::Comma);
      instruction->add_operand(parse_value_operand());
      break;
    }

    case Token::Keyword::Call: {
      parse_call_instruction(instruction.get());
      break;
    }

    case Token::Keyword::Branch: {
      instruction->add_operand(parse_block_operand());
      break;
    }

    case Token::Keyword::Bcond: {
      instruction->add_operand(parse_value_operand());
      lexer.consume_expect(Token::Kind::Comma);
      instruction->add_operand(parse_block_operand());
      lexer.consume_expect(Token::Kind::Comma);
      instruction->add_operand(parse_block_operand());
      break;
    }

    case Token::Keyword::Ret: {
      if (lexer.current_token().is_keyword(Token::Keyword::Void)) {
        lexer.consume_token();
      } else {
        instruction->add_operand(parse_value_operand());
      }
      break;
    }

    default: {
      fatal_error("Failed to parse void instruction: {}", Token::stringify_keyword(keyword));
    }
  }

  return instruction;
}

std::unique_ptr<PRInstruction> Parser::parse_nonvoid_instruction(std::string_view result_name) {
  verify(!result_name.empty(), "Result name cannot be empty for non-void instruction");

  const auto keyword = lexer.consume_keyword();

  auto instruction = std::make_unique<PRInstruction>();
  instruction->result_value = result_name;
  instruction->kind_kw = keyword;

  const auto parse_two_operands = [&](bool single_type) {
    instruction->add_operand(parse_value_operand());
    lexer.consume_expect(Token::Kind::Comma);

    if (single_type) {
      const auto type = instruction->operands.back().type;
      instruction->add_operand(parse_value_operand_with_type(type));
    } else {
      instruction->add_operand(parse_value_operand());
    }
  };

  if (Token::keyword_to_unary_op(keyword)) {
    instruction->add_operand(parse_value_operand());
    return instruction;
  }

  if (Token::keyword_to_binary_op(keyword)) {
    parse_two_operands(true);
    return instruction;
  }

  if (Token::keyword_to_cast(keyword)) {
    instruction->add_operand(parse_value_operand());
    lexer.consume_expect(Token::Keyword::To);
    instruction->specific_type = parse_type();

    return instruction;
  }

  switch (keyword) {
    case Token::Keyword::Cmp: {
      instruction->specific_keyword = lexer.consume_keyword();
      parse_two_operands(true);
      break;
    }

    case Token::Keyword::Load: {
      instruction->specific_type = parse_type();
      lexer.consume_expect(Token::Kind::Comma);
      instruction->add_operand(parse_value_operand());
      break;
    }

    case Token::Keyword::Call: {
      parse_call_instruction(instruction.get());
      break;
    }

    case Token::Keyword::Stackalloc: {
      instruction->specific_type = parse_type();
      instruction->specific_size = 1;

      if (lexer.current_token().is(Token::Kind::Comma)) {
        lexer.consume_token();
        instruction->specific_size = lexer.consume_token().get_literal();
      }
      break;
    }

    case Token::Keyword::Offset: {
      parse_two_operands(false);
      break;
    }

    case Token::Keyword::Select: {
      instruction->add_operand(parse_value_operand());
      lexer.consume_expect(Token::Kind::Comma);
      parse_two_operands(true);
      break;
    }

    case Token::Keyword::Phi: {
      instruction->specific_type = parse_type();
      parse_argument_list(Token::Kind::BracketOpen, Token::Kind::BracketClose, [&]() {
        instruction->add_operand(parse_block_operand());
        lexer.consume_expect(Token::Kind::Colon);
        instruction->add_operand(parse_value_operand_with_type(instruction->specific_type));
      });
      break;
    }

    default: {
      fatal_error("Failed to parse non-void instruction: {}", Token::stringify_keyword(keyword));
    }
  }

  return instruction;
}

std::unique_ptr<PRInstruction> Parser::parse_instruction() {
  if (lexer.current_token().is_identifier()) {
    const auto result_name = lexer.consume_identifier();
    lexer.consume_expect(Token::Kind::Assign);
    return parse_nonvoid_instruction(result_name);
  } else {
    return parse_void_instruction();
  }
}

void Parser::parse_function_body() {
  skip_newlines();
  lexer.consume_expect(Token::Kind::BraceOpen);
  skip_newlines();

  verify(!pr_block, "Cannot have active block at the beginning of function body parsing");

  while (!lexer.current_token().is(Token::Kind::BraceClose)) {
    skip_newlines();

    if (lexer.current_token().is_identifier()) {
      const auto block_name = lexer.consume_identifier();
      if (lexer.current_token().is(Token::Kind::Colon)) {
        lexer.consume_token();

        // New block.
        pr_function->blocks.push_back(std::make_unique<PRFunctionBlock>(block_name));
        pr_block = pr_function->blocks.back().get();

        continue;
      }

      lexer.restore(1);
    }

    verify(pr_block, "Instruction is not within a block");

    pr_block->instructions.push_back(parse_instruction());
    lexer.consume_expect(Token::Kind::NewLine);

    skip_newlines();
  }

  lexer.consume_token();

  pr_block = nullptr;
}

void Parser::parse_function() {
  skip_newlines();

  if (lexer.current_token().is(Token::Kind::Eof)) {
    return;
  }

  const bool is_extern = lexer.current_token().is_keyword(Token::Keyword::Extern);
  if (is_extern) {
    lexer.consume_token();
  }

  const auto return_type = parse_type();
  const auto function_name = lexer.consume_identifier();

  std::vector<PRFunctionParameter> parameters;

  parse_argument_list(Token::Kind::ParenOpen, Token::Kind::ParenClose, [&]() {
    const auto param_type = parse_type();
    const auto param_name = lexer.consume_identifier();
    parameters.push_back(PRFunctionParameter{param_type, param_name});
  });

  functions.push_back(
    std::make_unique<PRFunction>(is_extern, return_type, function_name, parameters));

  if (!is_extern) {
    pr_function = functions.back().get();
    parse_function_body();
    pr_function = nullptr;
  } else {
    lexer.consume_expect(Token::Kind::Semicolon);
  }
}

std::vector<std::unique_ptr<PRFunction>> Parser::parse() {
  while (!lexer.current_token().is(Token::Kind::Eof)) {
    parse_function();
  }

  return std::move(functions);
}