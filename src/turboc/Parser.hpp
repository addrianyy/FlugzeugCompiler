#pragma once
#include "AST.hpp"
#include "Lexer.hpp"

#include <Flugzeug/Core/Error.hpp>

#include <vector>

namespace turboc {

class Parser {
  Lexer& lexer;

  template <typename Fn> void parse_argument_list(Fn fn) {
    lexer.consume_expect(Token::Kind::ParenOpen);

    while (true) {
      if (lexer.current_token().is(Token::Kind::ParenClose)) {
        lexer.consume_token();
        break;
      }

      fn();

      const auto current = lexer.current_token();

      if (current.is(Token::Kind::Comma)) {
        lexer.consume_token();
      } else {
        verify(current.is(Token::Kind::ParenClose),
               "Expected comma or closing paren in argument list.");
      }
    }
  }

  Type parse_type();
  FunctionPrototype parse_prototype();


  Function parse_function();
  std::vector<Function> parse_functions();

  explicit Parser(Lexer& lexer) : lexer(lexer) {}

public:
  static std::vector<Function> parse(Lexer& lexer);
  static std::vector<Function> parse_from_file(const std::string& source_path);
};

} // namespace turboc