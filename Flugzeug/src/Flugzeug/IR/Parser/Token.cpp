#include "Token.hpp"

#include <Flugzeug/Core/StringifyEnum.hpp>

using namespace flugzeug;

// clang-format off
BEGIN_ENUM_STRINGIFY(stringify_token_kind, Token::Kind)
  ENUM_CASE(Keyword)
  ENUM_CASE(Identifier)
  ENUM_CASE(NumberLiteral)
  ENUM_CASE(Comma)
  ENUM_CASE(Colon)
  ENUM_CASE(Assign)
  ENUM_CASE(Star)
  ENUM_CASE(Semicolon)
  ENUM_CASE(ParenOpen)
  ENUM_CASE(ParenClose)
  ENUM_CASE(BracketOpen)
  ENUM_CASE(BracketClose)
  ENUM_CASE(BraceOpen)
  ENUM_CASE(BraceClose)
  ENUM_CASE(NewLine)
  ENUM_CASE(Eof)
END_ENUM_STRINGIFY()

BEGIN_ENUM_STRINGIFY(stringify_token_keyword, Token::Keyword)
  ENUM_CASE(I1)
  ENUM_CASE(I8)
  ENUM_CASE(I16)
  ENUM_CASE(I32)
  ENUM_CASE(I64)
  ENUM_CASE(Void)
  ENUM_CASE(Neg)
  ENUM_CASE(Not)
  ENUM_CASE(Add)
  ENUM_CASE(Sub)
  ENUM_CASE(Mul)
  ENUM_CASE(Smod)
  ENUM_CASE(Sdiv)
  ENUM_CASE(Umod)
  ENUM_CASE(Udiv)
  ENUM_CASE(Shr)
  ENUM_CASE(Shl)
  ENUM_CASE(Sar)
  ENUM_CASE(And)
  ENUM_CASE(Or)
  ENUM_CASE(Xor)
  ENUM_CASE(Cmp)
  ENUM_CASE(Eq)
  ENUM_CASE(Ne)
  ENUM_CASE(Ugt)
  ENUM_CASE(Ugte)
  ENUM_CASE(Sgt)
  ENUM_CASE(Sgte)
  ENUM_CASE(Ult)
  ENUM_CASE(Ulte)
  ENUM_CASE(Slt)
  ENUM_CASE(Slte)
  ENUM_CASE(To)
  ENUM_CASE(Zext)
  ENUM_CASE(Sext)
  ENUM_CASE(Trunc)
  ENUM_CASE(Bitcast)
  ENUM_CASE(Load)
  ENUM_CASE(Store)
  ENUM_CASE(Call)
  ENUM_CASE(Branch)
  ENUM_CASE(Bcond)
  ENUM_CASE(Stackalloc)
  ENUM_CASE(Ret)
  ENUM_CASE(Offset)
  ENUM_CASE(Select)
  ENUM_CASE(Phi)
  ENUM_CASE(True)
  ENUM_CASE(False)
  ENUM_CASE(Null)
  ENUM_CASE(Undef)
  ENUM_CASE(Extern)
END_ENUM_STRINGIFY()
// clang-format on

std::string_view Token::stringify_kind(Token::Kind kind) {
  return stringify_token_kind(kind);
}
std::string_view Token::stringify_keyword(Token::Keyword keyword) {
  return stringify_token_keyword(keyword);
}

Token::Keyword Token::get_keyword() const {
  verify(is_keyword(), "Called `get_keyword` on non-keyword");
  return keyword;
}

uint64_t Token::get_literal() const {
  verify(is_literal(), "Called `get_literal` on non-literal");
  return literal;
}

std::string_view Token::get_identifier() const {
  verify(is_identifier(), "Called `get_identifier` on non-identifier");
  return identifier;
}

std::optional<UnaryOp> Token::keyword_to_unary_op(Token::Keyword keyword) {
  // clang-format off
  switch (keyword) {
    case Token::Keyword::Neg: return UnaryOp::Neg;
    case Token::Keyword::Not: return UnaryOp::Not;
    default: return std::nullopt;
  }
  // clang-format on
}

std::optional<BinaryOp> Token::keyword_to_binary_op(Token::Keyword keyword) {
  // clang-format off
  switch (keyword) {
    case Token::Keyword::Add:  return BinaryOp::Add;
    case Token::Keyword::Sub:  return BinaryOp::Sub;
    case Token::Keyword::Mul:  return BinaryOp::Mul;
    case Token::Keyword::Umod: return BinaryOp::ModU;
    case Token::Keyword::Udiv: return BinaryOp::DivU;
    case Token::Keyword::Smod: return BinaryOp::ModS;
    case Token::Keyword::Sdiv: return BinaryOp::DivS;
    case Token::Keyword::Shr:  return BinaryOp::Shr;
    case Token::Keyword::Shl:  return BinaryOp::Shl;
    case Token::Keyword::Sar:  return BinaryOp::Sar;
    case Token::Keyword::And:  return BinaryOp::And;
    case Token::Keyword::Or:   return BinaryOp::Or;
    case Token::Keyword::Xor:  return BinaryOp::Xor;
    default: return std::nullopt;
  }
  // clang-format on
}

std::optional<IntPredicate> Token::keyword_to_int_predicate(Token::Keyword keyword) {
  // clang-format off
  switch (keyword) {
    case Token::Keyword::Eq:   return IntPredicate::Equal;
    case Token::Keyword::Ne:   return IntPredicate::NotEqual;
    case Token::Keyword::Ugt:  return IntPredicate::GtU;
    case Token::Keyword::Ugte: return IntPredicate::GteU;
    case Token::Keyword::Sgt:  return IntPredicate::GtS;
    case Token::Keyword::Sgte: return IntPredicate::GteS;
    case Token::Keyword::Ult:  return IntPredicate::LtU;
    case Token::Keyword::Ulte: return IntPredicate::LteU;
    case Token::Keyword::Slt:  return IntPredicate::LtS;
    case Token::Keyword::Slte: return IntPredicate::LteS;
    default: return std::nullopt;
  }
  // clang-format on
}

std::optional<CastKind> Token::keyword_to_cast(Token::Keyword keyword) {
  // clang-format off
  switch (keyword) {
    case Token::Keyword::Bitcast: return CastKind::Bitcast;
    case Token::Keyword::Zext:    return CastKind::ZeroExtend;
    case Token::Keyword::Sext:    return CastKind::SignExtend;
    case Token::Keyword::Trunc:   return CastKind::Truncate;
    default: return std::nullopt;
  }
  // clang-format on
}

std::string Token::format() const {
  std::string result = std::string(stringify_kind(kind_));
  if (kind_ == Kind::Keyword) {
    result = fmt::format("{}({})", result, stringify_keyword(keyword));
  }
  if (kind_ == Kind::NumberLiteral) {
    result = fmt::format("{}({})", result, literal);
  }
  if (kind_ == Kind::Identifier) {
    result = fmt::format("{}({})", result, identifier);
  }

  return result;
}