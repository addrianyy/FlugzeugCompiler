#pragma once
#include <optional>
#include <string_view>

#include <Flugzeug/IR/Instructions.hpp>

namespace flugzeug {

class Token {
  friend class Lexer;

public:
  enum class Keyword {
    I1,
    I8,
    I16,
    I32,
    I64,
    Void,

    Neg,
    Not,

    Add,
    Sub,
    Mul,
    Smod,
    Sdiv,
    Umod,
    Udiv,
    Shr,
    Shl,
    Sar,
    And,
    Or,
    Xor,

    Cmp,
    Eq,
    Ne,
    Ugt,
    Ugte,
    Sgt,
    Sgte,
    Ult,
    Ulte,
    Slt,
    Slte,

    To,
    Zext,
    Sext,
    Trunc,
    Bitcast,

    Load,
    Store,
    Call,
    Branch,
    Bcond,
    Stackalloc,
    Ret,
    Offset,
    Select,
    Phi,

    True,
    False,
    Null,
    Undef,
    Extern,
  };

  enum class Kind {
    Keyword,
    Identifier,
    NumberLiteral,

    Comma,
    Colon,
    Assign,
    Star,
    Semicolon,

    ParenOpen,
    ParenClose,
    BracketOpen,
    BracketClose,
    BraceOpen,
    BraceClose,

    NewLine,
    Eof,
  };

private:
  Kind kind;
  Keyword keyword{};
  uint64_t literal{};
  std::string_view identifier{};

  explicit Token(Kind kind) : kind(kind) {}

public:
  static std::string_view stringify_kind(Kind kind);
  static std::string_view stringify_keyword(Keyword keyword);

  static std::optional<UnaryOp> keyword_to_unary_op(Keyword keyword);
  static std::optional<BinaryOp> keyword_to_binary_op(Keyword keyword);
  static std::optional<IntPredicate> keyword_to_int_predicate(Keyword keyword);
  static std::optional<CastKind> keyword_to_cast(Keyword keyword);

  Kind get_kind() const { return kind; }
  bool is(Kind checked_kind) const { return kind == checked_kind; }

  bool is_keyword() const { return is(Kind::Keyword); }
  bool is_literal() const { return is(Kind::NumberLiteral); }
  bool is_identifier() const { return is(Kind::Identifier); }

  bool is_keyword(Keyword kw) const { return is(Kind::Keyword) && keyword == kw; }

  Keyword get_keyword() const;
  uint64_t get_literal() const;
  std::string_view get_identifier() const;

  std::string format() const;
};

} // namespace flugzeug