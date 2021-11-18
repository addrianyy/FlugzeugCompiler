#include "Lexer.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace turboc;
using namespace std::literals;

#define BEGIN_ENUM_STRINGIFY(function_name, enum_type)                                             \
  static std::string_view function_name(enum_type e) {                                             \
    using ThisEnum = enum_type;                                                                    \
    switch (e) {

#define ENUM_CASE(enum_case)                                                                       \
  case ThisEnum::enum_case:                                                                        \
    return #enum_case;

#define END_ENUM_STRINGIFY()                                                                       \
  default:                                                                                         \
    unreachable();                                                                                 \
    }                                                                                              \
    }

// clang-format off
BEGIN_ENUM_STRINGIFY(stringify_token_kind, Token::Kind)
  ENUM_CASE(Keyword)
  ENUM_CASE(Identifier)
  ENUM_CASE(NumberLiteral)
  ENUM_CASE(StringLiteral)
  ENUM_CASE(Colon)
  ENUM_CASE(Semicolon)
  ENUM_CASE(Comma)
  ENUM_CASE(ParenOpen)
  ENUM_CASE(ParenClose)
  ENUM_CASE(BraceOpen)
  ENUM_CASE(BraceClose)
  ENUM_CASE(BracketOpen)
  ENUM_CASE(BracketClose)
  ENUM_CASE(Add)
  ENUM_CASE(Sub)
  ENUM_CASE(Mul)
  ENUM_CASE(Mod)
  ENUM_CASE(Div)
  ENUM_CASE(Shr)
  ENUM_CASE(Shl)
  ENUM_CASE(And)
  ENUM_CASE(Or)
  ENUM_CASE(Xor)
  ENUM_CASE(Not)
  ENUM_CASE(Assign)
  ENUM_CASE(AddAssign)
  ENUM_CASE(SubAssign)
  ENUM_CASE(MulAssign)
  ENUM_CASE(ModAssign)
  ENUM_CASE(DivAssign)
  ENUM_CASE(ShrAssign)
  ENUM_CASE(ShlAssign)
  ENUM_CASE(AndAssign)
  ENUM_CASE(OrAssign)
  ENUM_CASE(XorAssign)
  ENUM_CASE(Equal)
  ENUM_CASE(NotEqual)
  ENUM_CASE(Gt)
  ENUM_CASE(Lt)
  ENUM_CASE(Gte)
  ENUM_CASE(Lte)
  ENUM_CASE(Eof)
END_ENUM_STRINGIFY()
// clang-format on

// clang-format off
BEGIN_ENUM_STRINGIFY(stringify_token_keyword, Token::Keyword)
  ENUM_CASE(Void)
  ENUM_CASE(U8)
  ENUM_CASE(U16)
  ENUM_CASE(U32)
  ENUM_CASE(U64)
  ENUM_CASE(I8)
  ENUM_CASE(I16)
  ENUM_CASE(I32)
  ENUM_CASE(I64)
  ENUM_CASE(For)
  ENUM_CASE(While)
  ENUM_CASE(If)
  ENUM_CASE(Else)
  ENUM_CASE(Break)
  ENUM_CASE(Continue)
  ENUM_CASE(Return)
  ENUM_CASE(Extern)
END_ENUM_STRINGIFY()
// clang-format on

// clang-format off
BEGIN_ENUM_STRINGIFY(strinigy_token_type_override, Token::TypeOverride)
  ENUM_CASE(I8)
  ENUM_CASE(I16)
  ENUM_CASE(I32)
  ENUM_CASE(I64)
  ENUM_CASE(U8)
  ENUM_CASE(U16)
  ENUM_CASE(U32)
  ENUM_CASE(U64)
END_ENUM_STRINGIFY()
// clang-format on

std::string Token::format() const {
  std::string result = std::string(stringify_token_kind(kind));

  const bool more_information = kind == Kind::Keyword || kind == Kind::Identifier ||
                                kind == Kind::NumberLiteral || kind == Kind::StringLiteral;

  if (more_information) {
    result += "(";

    if (kind == Token::Kind::Identifier) {
      result += identifier;
    } else if (kind == Token::Kind::Keyword) {
      result += stringify_token_keyword(keyword);
    } else if (kind == Token::Kind::NumberLiteral) {
      result += std::to_string(number_literal.literal);
      if (number_literal.type_override) {
        result += ", type override ";
        result += strinigy_token_type_override(*number_literal.type_override);
      }
    } else if (kind == Kind::StringLiteral) {
      unreachable();
    }

    result += ")";
  }

  return result;
}