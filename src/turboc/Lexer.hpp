#pragma once
#include <Flugzeug/Core/ClassTraits.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace turboc {

class Token {
  friend class Lexer;

public:
  enum class Kind {
    Keyword,
    Identifier,
    NumberLiteral,
    StringLiteral,

    Colon,
    Semicolon,
    Comma,

    ParenOpen,
    ParenClose,
    BraceOpen,
    BraceClose,
    BracketOpen,
    BracketClose,

    Add,
    Sub,
    Mul,
    Mod,
    Div,
    Shr,
    Shl,
    And,
    Or,
    Xor,
    Not,
    Assign,

    AddAssign,
    SubAssign,
    MulAssign,
    ModAssign,
    DivAssign,
    ShrAssign,
    ShlAssign,
    AndAssign,
    OrAssign,
    XorAssign,

    Equal,
    NotEqual,
    Gt,
    Lt,
    Gte,
    Lte,

    Eof,
  };

  enum class Keyword {
    Void,
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    For,
    While,
    If,
    Else,
    Break,
    Continue,
    Return,
    Extern,
  };

  enum class TypeOverride {
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
  };

  struct NumberLiteral {
    uint64_t literal{};
    std::optional<TypeOverride> type_override;
    uint32_t base{};
  };

private:
  Kind kind;

  Keyword keyword{};
  std::string_view identifier{};
  NumberLiteral number_literal{};

  explicit Token(Kind kind) : kind(kind) {}

public:
  Kind get_kind() const { return kind; }
  bool is(Kind wanted_kind) const { return kind == wanted_kind; }
  bool is_keyword(Keyword wanted_keyword) const {
    return is(Kind::Keyword) && keyword == wanted_keyword;
  }

  bool is_keyword() const { return is(Kind::Keyword); }
  bool is_identifier() const { return is(Kind::Identifier); }
  bool is_number_literal() const { return is(Kind::NumberLiteral); }

  Keyword get_keyword() const;
  std::string_view get_identifier() const;
  const NumberLiteral& get_number_literal() const;

  std::string format() const;
};

class Lexer {
  std::string source_buffer;
  std::vector<Token> tokens;

  Token eof;

  intptr_t cursor = 0;

  static Token lex_number_literal(std::string_view& source);
  static Token lex_char_literal(std::string_view& source);
  static Token lex_string_literal(std::string_view& source);

  static bool skip_comments(std::string_view& source);
  static std::optional<Token> lex_static_token(std::string_view& source);
  static std::optional<Token> lex_literal(std::string_view& source);
  static Token lex_identifier(std::string_view& source);

  void lex();

  const Token& get_token(intptr_t token_cursor) const;

public:
  // Tokens contain references to `source_buffer`.
  CLASS_NON_MOVABLE_NON_COPYABLE(Lexer)

  static Lexer from_file(const std::string& path);

  explicit Lexer(std::string source);

  void print_tokens();

  const Token& current_token() const;
  const Token& consume_token();

  void restore(size_t count);

  std::string_view consume_identifier();
  Token::Keyword consume_keyword();

  void consume_expect(Token::Kind expected_kind);
  void consume_expect(Token::Keyword expected_keyword);
};

} // namespace turboc
