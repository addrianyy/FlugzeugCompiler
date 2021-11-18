#pragma once
#include "AST.hpp"
#include "Function.hpp"
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

  std::unique_ptr<Expr> parse_binary_expression(int32_t min_precedence, std::unique_ptr<Expr> expr);
  std::unique_ptr<Expr> parse_unary_expression(UnaryOp op);
  std::unique_ptr<Expr> parse_call_expression(const std::string& function);
  std::unique_ptr<Expr> parse_number_expression();
  std::unique_ptr<Expr> parse_primary_expression();
  std::unique_ptr<Expr> parse_paren_expression();
  std::unique_ptr<Expr> parse_expression();

  std::unique_ptr<Stmt> parse_expression_statement();
  std::unique_ptr<Stmt> parse_declaration();
  std::unique_ptr<Stmt> parse_if();
  std::unique_ptr<Stmt> parse_for();
  std::unique_ptr<Stmt> parse_statement();
  std::unique_ptr<BodyStmt> parse_body();

  FunctionPrototype parse_prototype();
  Function parse_function();

  std::vector<Function> parse_functions();

  explicit Parser(Lexer& lexer) : lexer(lexer) {}

public:
  static std::vector<Function> parse(Lexer& lexer);
  static std::vector<Function> parse_from_file(const std::string& source_path);
};

} // namespace turboc