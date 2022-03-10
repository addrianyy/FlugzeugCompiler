#include "Lexer.hpp"

#include <Flugzeug/Core/Error.hpp>
#include <Flugzeug/Core/Files.hpp>

#include <array>
#include <fstream>
#include <iostream>

using namespace turboc;
using namespace std::literals;

Token::Keyword Token::get_keyword() const {
  verify(kind == Kind::Keyword, "This is not a keyword token");
  return keyword;
}

std::string_view Token::get_identifier() const {
  verify(kind == Kind::Identifier, "This is not a identifier token");
  return identifier;
}

const Token::NumberLiteral& Token::get_number_literal() const {
  verify(kind == Kind::NumberLiteral, "This is not a number literal token");
  return number_literal;
}

static void trim_start(std::string_view& string) {
  const auto it =
    std::find_if(string.begin(), string.end(), [](char c) { return !std::isspace(c); });

  if (it != string.begin()) {
    string = string.substr(std::distance(string.begin(), it));
  }
}

Token Lexer::lex_number_literal(std::string_view& source) {
  constexpr static std::string_view valid_bin = "01";
  constexpr static std::string_view valid_dec = "0123456789";
  constexpr static std::string_view valid_hex = "0123456789abcdefABCDEF";

  constexpr static auto type_overrides = std::array{
    std::pair{"u8"sv, Token::TypeOverride::U8},   std::pair{"u16"sv, Token::TypeOverride::U16},
    std::pair{"u32"sv, Token::TypeOverride::U32}, std::pair{"u64"sv, Token::TypeOverride::U64},
    std::pair{"i8"sv, Token::TypeOverride::I8},   std::pair{"i16"sv, Token::TypeOverride::I16},
    std::pair{"i32"sv, Token::TypeOverride::I32}, std::pair{"i64"sv, Token::TypeOverride::I64},
  };

  std::string_view valid_chars = valid_dec;
  uint32_t base = 10;

  if (source.starts_with("0x")) {
    valid_chars = valid_hex;
    base = 16;

    source = source.substr(2);
  } else if (source.starts_with("0b")) {
    valid_chars = valid_bin;
    base = 2;

    source = source.substr(2);
  }

  std::string literal;

  size_t index = 0;
  for (; index < source.size(); ++index) {
    const char c = source[index];
    if (c == '_') {
      continue;
    }

    if (valid_chars.find(c) != std::string_view::npos) {
      literal.push_back(c);
    } else {
      break;
    }
  }

  source = source.substr(index);

  std::optional<Token::TypeOverride> type_override;

  for (const auto [string, override] : type_overrides) {
    if (source.starts_with(string)) {
      source = source.substr(string.size());
      type_override = override;
      break;
    }
  }

  const uint64_t number = std::strtoull(literal.c_str(), nullptr, int(base));

  Token token(Token::Kind::NumberLiteral);
  token.number_literal = Token::NumberLiteral{number, type_override, base};

  return token;
}

Token Lexer::lex_char_literal(std::string_view& source) {
  const char c = source[1];

  verify(source[2] == '\'', "Invalid character literal");

  Token token(Token::Kind::NumberLiteral);
  token.number_literal = Token::NumberLiteral{uint8_t(c), Token::TypeOverride::U8};

  return token;
}

Token Lexer::lex_string_literal(std::string_view& source) { fatal_error("Not implemented yet."); }

bool Lexer::skip_comments(std::string_view& source) {
  if (source.starts_with("/*"sv)) {
    const auto end_comment = source.find("*/"sv);
    verify(end_comment != std::string_view::npos, "No end of comment found");

    source = source.substr(end_comment + 2);
    return true;
  }

  if (source.starts_with("//"sv)) {
    const auto end_comment = source.find('\n');

    source =
      end_comment == std::string_view::npos ? std::string_view() : source.substr(end_comment + 1);
    return true;
  }

  return false;
}

std::optional<Token> Lexer::lex_static_token(std::string_view& source) {
  constexpr static auto static_tokens = std::array{
    std::pair{">>="sv, Token::Kind::ShrAssign}, std::pair{"<<="sv, Token::Kind::ShlAssign},
    std::pair{"=="sv, Token::Kind::Equal},      std::pair{"!="sv, Token::Kind::NotEqual},
    std::pair{">="sv, Token::Kind::Gte},        std::pair{"<="sv, Token::Kind::Lte},
    std::pair{"+="sv, Token::Kind::AddAssign},  std::pair{"-="sv, Token::Kind::SubAssign},
    std::pair{"*="sv, Token::Kind::MulAssign},  std::pair{"%="sv, Token::Kind::ModAssign},
    std::pair{"/="sv, Token::Kind::DivAssign},  std::pair{"&="sv, Token::Kind::AndAssign},
    std::pair{"|="sv, Token::Kind::OrAssign},   std::pair{"^="sv, Token::Kind::XorAssign},
    std::pair{">>"sv, Token::Kind::Shr},        std::pair{"<<"sv, Token::Kind::Shl},
    std::pair{"+"sv, Token::Kind::Add},         std::pair{"-"sv, Token::Kind::Sub},
    std::pair{"*"sv, Token::Kind::Mul},         std::pair{"%"sv, Token::Kind::Mod},
    std::pair{"/"sv, Token::Kind::Div},         std::pair{"&"sv, Token::Kind::And},
    std::pair{"|"sv, Token::Kind::Or},          std::pair{"^"sv, Token::Kind::Xor},
    std::pair{"~"sv, Token::Kind::Not},         std::pair{"="sv, Token::Kind::Assign},
    std::pair{"("sv, Token::Kind::ParenOpen},   std::pair{")"sv, Token::Kind::ParenClose},
    std::pair{"{"sv, Token::Kind::BraceOpen},   std::pair{"}"sv, Token::Kind::BraceClose},
    std::pair{"["sv, Token::Kind::BracketOpen}, std::pair{"]"sv, Token::Kind::BracketClose},
    std::pair{","sv, Token::Kind::Comma},       std::pair{":"sv, Token::Kind::Colon},
    std::pair{";"sv, Token::Kind::Semicolon},   std::pair{">"sv, Token::Kind::Gt},
    std::pair{"<"sv, Token::Kind::Lt},
  };

  for (const auto [string, kind] : static_tokens) {
    if (source.starts_with(string)) {
      source = source.substr(string.size());
      return Token(kind);
    }
  }

  return std::nullopt;
}

std::optional<Token> Lexer::lex_literal(std::string_view& source) {
  const char start = source[0];

  if (start == '\'') {
    return lex_char_literal(source);
  } else if (start == '\"') {
    return lex_string_literal(source);
  } else if (std::isdigit(start)) {
    return lex_number_literal(source);
  }

  return std::nullopt;
}

Token Lexer::lex_identifier(std::string_view& source) {
  const auto identifier_end_it =
    std::find_if(source.begin(), source.end(), [](char c) { return c != '_' && !std::isalnum(c); });
  const auto identifier_end = std::distance(source.begin(), identifier_end_it);

  const auto identifier = source.substr(0, identifier_end);
  verify(!identifier.empty(), "Parsed empty identifier");

  source = source.substr(identifier_end);

  constexpr static auto keywords = std::array{
    std::pair{"void"sv, Token::Keyword::Void},
    std::pair{"u8"sv, Token::Keyword::U8},
    std::pair{"u16"sv, Token::Keyword::U16},
    std::pair{"u32"sv, Token::Keyword::U32},
    std::pair{"u64"sv, Token::Keyword::U64},
    std::pair{"i8"sv, Token::Keyword::I8},
    std::pair{"i16"sv, Token::Keyword::I16},
    std::pair{"i32"sv, Token::Keyword::I32},
    std::pair{"i64"sv, Token::Keyword::I64},
    std::pair{"for"sv, Token::Keyword::For},
    std::pair{"while"sv, Token::Keyword::While},
    std::pair{"if"sv, Token::Keyword::If},
    std::pair{"else"sv, Token::Keyword::Else},
    std::pair{"break"sv, Token::Keyword::Break},
    std::pair{"continue"sv, Token::Keyword::Continue},
    std::pair{"return"sv, Token::Keyword::Return},
    std::pair{"extern"sv, Token::Keyword::Extern},
  };

  for (const auto [string, keyword] : keywords) {
    if (identifier == string) {
      Token token(Token::Kind::Keyword);
      token.keyword = keyword;

      return token;
    }
  }

  Token token(Token::Kind::Identifier);
  token.identifier = identifier;

  return token;
}

Lexer Lexer::from_file(const std::string& path) {
  return Lexer(flugzeug::read_file_to_string(path));
}

void Lexer::lex() {
  std::string_view source = source_buffer;

  while (!source.empty()) {
    trim_start(source);
    if (source.empty()) {
      break;
    }

    if (skip_comments(source)) {
      continue;
    }

    if (const auto token = lex_static_token(source)) {
      tokens.push_back(*token);
      continue;
    }

    if (const auto token = lex_literal(source)) {
      tokens.push_back(*token);
      continue;
    }

    tokens.push_back(lex_identifier(source));
  }
}

const Token& Lexer::get_token(intptr_t token_cursor) const {
  if (token_cursor < 0 || token_cursor >= intptr_t(tokens.size())) {
    return eof;
  }

  return tokens[token_cursor];
}

Lexer::Lexer(std::string source) : source_buffer(std::move(source)), eof(Token::Kind::Eof) {
  lex();
}

void Lexer::print_tokens() {
  for (const Token& token : tokens) {
    std::cout << token.format();

    if (&token != &tokens.back()) {
      std::cout << '\n';
    }
  }

  std::cout << std::endl;
}

const Token& Lexer::current_token() const { return get_token(cursor); }

const Token& Lexer::consume_token() {
  const auto prev_cursor = cursor;
  cursor++;
  return get_token(prev_cursor);
}

void Lexer::restore(size_t count) { cursor -= intptr_t(count); }

std::string_view Lexer::consume_identifier() {
  const auto token = consume_token();

  if (token.is_identifier()) {
    return token.get_identifier();
  }

  fatal_error("Expected identifier, got {}.", token.format());
}

Token::Keyword Lexer::consume_keyword() {
  const auto token = consume_token();

  if (token.is_keyword()) {
    return token.get_keyword();
  }

  fatal_error("Expected keyword, got {}.", token.format());
}

void Lexer::consume_expect(Token::Kind expected_kind) {
  const auto token = consume_token();
  verify(token.is(expected_kind), "Unexpected token {}.", token.format());
}

void Lexer::consume_expect(Token::Keyword expected_keyword) {
  const auto token = consume_token();
  verify(token.is(Token::Kind::Keyword) && token.get_keyword() == expected_keyword,
         "Unexpected token {}.", token.format());
}