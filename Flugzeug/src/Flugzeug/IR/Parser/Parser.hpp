#pragma once
#include "AST.hpp"
#include "Lexer.hpp"

#include <Flugzeug/IR/Module.hpp>

namespace flugzeug {

class Parser {
  Lexer& lexer;

  PRFunction* pr_function = nullptr;
  PRFunctionBlock* pr_block = nullptr;

  std::vector<std::unique_ptr<PRFunction>> functions;

  template <typename Fn>
  void parse_argument_list(Token::Kind start_token, Token::Kind end_token, Fn fn) {
    lexer.consume_expect(start_token);

    while (true) {
      if (lexer.current_token().is(end_token)) {
        lexer.consume_token();
        break;
      }

      fn();

      const auto current = lexer.current_token();

      if (current.is(Token::Kind::Comma)) {
        lexer.consume_token();
      } else {
        verify(current.is(end_token), "Expected comma or closing paren in argument list.");
      }
    }
  }

  void skip_newlines();

  PRType parse_type();

  PRInstructionOperand parse_value_operand_with_type(PRType type);
  PRInstructionOperand parse_value_operand();

  PRInstructionOperand parse_block_operand();

  void parse_call_instruction(PRInstruction* instruction);

  std::unique_ptr<PRInstruction> parse_void_instruction();
  std::unique_ptr<PRInstruction> parse_nonvoid_instruction(std::string_view result_name);
  std::unique_ptr<PRInstruction> parse_instruction();

  void parse_function_body();
  void parse_function();

 public:
  explicit Parser(Lexer& lexer) : lexer(lexer) {}

  std::vector<std::unique_ptr<PRFunction>> parse();
};

}  // namespace flugzeug