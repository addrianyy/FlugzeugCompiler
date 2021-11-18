#include "AST.hpp"

using namespace turboc;

std::optional<Type::Kind> turboc::conv::keyword_to_type_kind(Token::Keyword keyword) {
  // clang-format off
  switch (keyword) {
  case Token::Keyword::U8:   return Type::Kind::U8;
  case Token::Keyword::U16:  return Type::Kind::U16;
  case Token::Keyword::U32:  return Type::Kind::U32;
  case Token::Keyword::U64:  return Type::Kind::U64;
  case Token::Keyword::I8:   return Type::Kind::I8;
  case Token::Keyword::I16:  return Type::Kind::I16;
  case Token::Keyword::I32:  return Type::Kind::I32;
  case Token::Keyword::I64:  return Type::Kind::I64;
  case Token::Keyword::Void: return Type::Kind::Void;
  default:                   return std::nullopt;
  }
  // clang-format on
}
