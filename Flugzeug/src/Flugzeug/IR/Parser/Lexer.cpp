#include "Lexer.hpp"

#include <Flugzeug/Core/Error.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>

using namespace std::literals;
using namespace flugzeug;

static void trim_start(std::string_view& source) {
  const auto it = std::find_if(source.begin(), source.end(),
                               [](char c) { return !std::isspace(c) || c == '\n'; });

  if (it != source.begin()) {
    source = source.substr(std::distance(source.begin(), it));
  }
}

static bool skip_comments(std::string_view& source) {
  if (source.starts_with("/*"sv)) {
    const auto end_comment = source.find("*/"sv);
    verify(end_comment != std::string_view::npos, "No end of comment found");

    source = source.substr(end_comment + 2);
    return true;
  }

  if (source.starts_with("//"sv)) {
    const auto end_comment = source.find('\n');

    // Keep the newline.
    source =
      end_comment == std::string_view::npos ? std::string_view() : source.substr(end_comment);
    return true;
  }

  return false;
}

static std::optional<Token::Kind> lex_static_tokens(std::string_view& source,
                                                    bool advance_source = true) {
  constexpr static auto static_tokens = std::array{
    std::pair{","sv, Token::Kind::Comma},        std::pair{":"sv, Token::Kind::Colon},
    std::pair{"="sv, Token::Kind::Assign},       std::pair{"*"sv, Token::Kind::Star},
    std::pair{";"sv, Token::Kind::Semicolon},    std::pair{"("sv, Token::Kind::ParenOpen},
    std::pair{")"sv, Token::Kind::ParenClose},   std::pair{"["sv, Token::Kind::BracketOpen},
    std::pair{"]"sv, Token::Kind::BracketClose}, std::pair{"{"sv, Token::Kind::BraceOpen},
    std::pair{"}"sv, Token::Kind::BraceClose},   std::pair{"\n"sv, Token::Kind::NewLine},
  };

  for (const auto [string, kind] : static_tokens) {
    if (source.starts_with(string)) {
      if (advance_source) {
        source = source.substr(string.size());
      }
      return kind;
    }
  }

  return std::nullopt;
}

static std::optional<uint64_t> lex_number_literal(std::string_view& source) {
  bool negate = false;
  if (source.starts_with("-")) {
    source = source.substr(1);
    negate = true;
  }

  verify(!source.empty(), "Source is empty when lexing number literal");
  if (!std::isdigit(source[0])) {
    verify(!negate, "Expected digit after unary minus");
    return std::nullopt;
  }

  int base = 10;
  if (source.starts_with("0x") || source.starts_with("0X")) {
    source = source.substr(2);
    base = 16;
  }

  const auto end_it = std::find_if(source.begin(), source.end(), [base](char c) {
    return base == 16 ? !std::isxdigit(c) : !std::isdigit(c);
  });
  verify(end_it != source.end(), "No end of number literal found");
  const auto end = std::distance(source.begin(), end_it);

  const auto literal_str = std::string(source.substr(0, end));

  source = source.substr(end);
  if (!(source.empty() || std::isspace(source[0]) || !!lex_static_tokens(source, false))) {
    fatal_error("Invalid string literal");
  }

  uint64_t literal = std::strtoull(literal_str.c_str(), nullptr, base);
  if (negate) {
    literal = uint64_t(-int64_t(literal));
  }

  return literal;
}

static std::string_view lex_identifier(std::string_view& source) {
  const auto identifier_end_it =
    std::find_if(source.begin(), source.end(), [](char c) { return c != '_' && !std::isalnum(c); });
  const auto identifier_end = std::distance(source.begin(), identifier_end_it);

  const auto identifier = source.substr(0, identifier_end);
  verify(!identifier.empty(), "Parsed empty identifier");
  verify(!std::isdigit(identifier[0]), "Identifier cannot start with a number");

  source = source.substr(identifier_end);

  return identifier;
}

static std::optional<Token::Keyword> keyword_from_identifer(std::string_view identifier) {
  constexpr static auto keywords = std::array{
    std::pair{"i1"sv, Token::Keyword::I1},
    std::pair{"i8"sv, Token::Keyword::I8},
    std::pair{"i16"sv, Token::Keyword::I16},
    std::pair{"i32"sv, Token::Keyword::I32},
    std::pair{"i64"sv, Token::Keyword::I64},
    std::pair{"void"sv, Token::Keyword::Void},
    std::pair{"neg"sv, Token::Keyword::Neg},
    std::pair{"not"sv, Token::Keyword::Not},
    std::pair{"add"sv, Token::Keyword::Add},
    std::pair{"sub"sv, Token::Keyword::Sub},
    std::pair{"mul"sv, Token::Keyword::Mul},
    std::pair{"smod"sv, Token::Keyword::Smod},
    std::pair{"sdiv"sv, Token::Keyword::Sdiv},
    std::pair{"umod"sv, Token::Keyword::Umod},
    std::pair{"udiv"sv, Token::Keyword::Udiv},
    std::pair{"shr"sv, Token::Keyword::Shr},
    std::pair{"shl"sv, Token::Keyword::Shl},
    std::pair{"sar"sv, Token::Keyword::Sar},
    std::pair{"and"sv, Token::Keyword::And},
    std::pair{"or"sv, Token::Keyword::Or},
    std::pair{"xor"sv, Token::Keyword::Xor},
    std::pair{"cmp"sv, Token::Keyword::Cmp},
    std::pair{"eq"sv, Token::Keyword::Eq},
    std::pair{"ne"sv, Token::Keyword::Ne},
    std::pair{"ugt"sv, Token::Keyword::Ugt},
    std::pair{"ugte"sv, Token::Keyword::Ugte},
    std::pair{"sgt"sv, Token::Keyword::Sgt},
    std::pair{"sgte"sv, Token::Keyword::Sgte},
    std::pair{"ult"sv, Token::Keyword::Ult},
    std::pair{"ulte"sv, Token::Keyword::Ulte},
    std::pair{"slt"sv, Token::Keyword::Slt},
    std::pair{"slte"sv, Token::Keyword::Slte},
    std::pair{"to"sv, Token::Keyword::To},
    std::pair{"zext"sv, Token::Keyword::Zext},
    std::pair{"sext"sv, Token::Keyword::Sext},
    std::pair{"trunc"sv, Token::Keyword::Trunc},
    std::pair{"bitcast"sv, Token::Keyword::Bitcast},
    std::pair{"load"sv, Token::Keyword::Load},
    std::pair{"store"sv, Token::Keyword::Store},
    std::pair{"call"sv, Token::Keyword::Call},
    std::pair{"branch"sv, Token::Keyword::Branch},
    std::pair{"bcond"sv, Token::Keyword::Bcond},
    std::pair{"stackalloc"sv, Token::Keyword::Stackalloc},
    std::pair{"ret"sv, Token::Keyword::Ret},
    std::pair{"offset"sv, Token::Keyword::Offset},
    std::pair{"select"sv, Token::Keyword::Select},
    std::pair{"phi"sv, Token::Keyword::Phi},
    std::pair{"true"sv, Token::Keyword::True},
    std::pair{"false"sv, Token::Keyword::False},
    std::pair{"null"sv, Token::Keyword::Null},
    std::pair{"undef"sv, Token::Keyword::Undef},
    std::pair{"extern"sv, Token::Keyword::Extern},
  };

  for (const auto [string, keyword] : keywords) {
    if (identifier == string) {
      return keyword;
    }
  }

  return std::nullopt;
}

const Token& Lexer::get_token(intptr_t token_cursor) const {
  if (token_cursor < 0 || token_cursor >= intptr_t(tokens.size())) {
    return eof;
  }

  return tokens[token_cursor];
}

Lexer::Lexer(std::string source_to_lex) : source(std::move(source_to_lex)), eof(Token::Kind::Eof) {
  std::string_view string = source;

  while (!string.empty()) {
    trim_start(string);
    if (string.empty()) {
      continue;
    }

    if (skip_comments(string)) {
      continue;
    }

    if (const auto static_token = lex_static_tokens(string)) {
      tokens.push_back(Token(*static_token));
      continue;
    }

    if (const auto number_literal = lex_number_literal(string)) {
      Token token(Token::Kind::NumberLiteral);
      token.literal = *number_literal;

      tokens.push_back(token);
      continue;
    }

    const auto identifier = lex_identifier(string);
    if (const auto keyword = keyword_from_identifer(identifier)) {
      Token token(Token::Kind::Keyword);
      token.keyword = *keyword;

      tokens.push_back(token);
    } else {
      Token token(Token::Kind::Identifier);
      token.identifier = identifier;

      tokens.push_back(token);
    }
  }
}

void Lexer::restore(size_t count) { cursor -= intptr_t(count); }

const Token& Lexer::current_token() const { return get_token(cursor); }
const Token& Lexer::consume_token() {
  const auto prev_cursor = cursor;
  cursor++;
  return get_token(prev_cursor);
}

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
  verify(token.is(expected_kind), "Unexpected token {}", token.format());
}

void Lexer::consume_expect(Token::Keyword expected_keyword) {
  const auto token = consume_token();
  verify(token.is(Token::Kind::Keyword) && token.get_keyword() == expected_keyword,
         "Unexpected token {}", token.format());
}

void Lexer::print_tokens() const {
  for (const Token& token : tokens) {
    std::cout << token.format();

    if (&token != &tokens.back()) {
      std::cout << '\n';
    }
  }

  std::cout << std::endl;
}