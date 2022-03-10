#pragma once
#include "Token.hpp"
#include <string>
#include <vector>

namespace flugzeug {

class Lexer {
  std::string source;
  std::vector<Token> tokens;

  Token eof;
  intptr_t cursor = 0;

  const Token& get_token(intptr_t token_cursor) const;

public:
  explicit Lexer(std::string source_to_lex);

  void restore(size_t count);

  const Token& current_token() const;
  const Token& consume_token();

  std::string_view consume_identifier();
  Token::Keyword consume_keyword();

  void consume_expect(Token::Kind expected_kind);
  void consume_expect(Token::Keyword expected_keyword);

  void print_tokens() const;
};

} // namespace flugzeug