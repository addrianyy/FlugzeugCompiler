#include "Conversion.hpp"
#include <Flugzeug/Core/Error.hpp>

using namespace turboc;

std::optional<Type::Kind> conv::keyword_to_type_kind(Token::Keyword keyword) {
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

std::optional<UnaryOp> conv::token_to_unary_op(Token::Kind kind) {
  // clang-format off
  switch (kind) {
  case Token::Kind::Sub: return UnaryOp::Neg;
  case Token::Kind::Not: return UnaryOp::Not;
  case Token::Kind::And: return UnaryOp::Ref;
  case Token::Kind::Mul: return UnaryOp::Deref;
  default:               return std::nullopt;
  }
  // clang-format on
}

std::optional<BinaryOp> conv::token_to_binary_op(Token::Kind kind) {
  // clang-format off
  switch (kind) {
  case Token::Kind::Add:       return BinaryOp::Add;
  case Token::Kind::Sub:       return BinaryOp::Sub;
  case Token::Kind::Mul:       return BinaryOp::Mul;
  case Token::Kind::Div:       return BinaryOp::Div;
  case Token::Kind::Mod:       return BinaryOp::Mod;
  case Token::Kind::Shr:       return BinaryOp::Shr;
  case Token::Kind::Shl:       return BinaryOp::Shl;
  case Token::Kind::And:       return BinaryOp::And;
  case Token::Kind::Or:        return BinaryOp::Or;
  case Token::Kind::Xor:       return BinaryOp::Xor;
  case Token::Kind::Equal:     return BinaryOp::Equal;
  case Token::Kind::NotEqual:  return BinaryOp::NotEqual;
  case Token::Kind::Gt:        return BinaryOp::Gt;
  case Token::Kind::Lt:        return BinaryOp::Lt;
  case Token::Kind::Gte:       return BinaryOp::Gte;
  case Token::Kind::Lte:       return BinaryOp::Lte;
  default:                     return std::nullopt;
  }
  // clang-format on
}

std::optional<BinaryOp> conv::binary_op_for_binary_assign(Token::Kind kind) {
  // clang-format off
  switch (kind) {
  case Token::Kind::AddAssign: return BinaryOp::Add;
  case Token::Kind::SubAssign: return BinaryOp::Sub;
  case Token::Kind::MulAssign: return BinaryOp::Mul;
  case Token::Kind::ModAssign: return BinaryOp::Mod;
  case Token::Kind::DivAssign: return BinaryOp::Div;
  case Token::Kind::ShrAssign: return BinaryOp::Shr;
  case Token::Kind::ShlAssign: return BinaryOp::Shl;
  case Token::Kind::AndAssign: return BinaryOp::And;
  case Token::Kind::OrAssign:  return BinaryOp::Or;
  case Token::Kind::XorAssign: return BinaryOp::Xor;
  default:                     return std::nullopt;
  }
  // clang-format on
}

Type::Kind conv::type_override_to_type(Token::TypeOverride override) {
  // clang-format off
  switch (override) {
  case Token::TypeOverride::U8:  return Type::Kind::U8;
  case Token::TypeOverride::U16: return Type::Kind::U16;
  case Token::TypeOverride::U32: return Type::Kind::U32;
  case Token::TypeOverride::U64: return Type::Kind::U64;
  case Token::TypeOverride::I8:  return Type::Kind::I8;
  case Token::TypeOverride::I16: return Type::Kind::I16;
  case Token::TypeOverride::I32: return Type::Kind::I32;
  case Token::TypeOverride::I64: return Type::Kind::I64;
  default:                       unreachable();
  }
  // clang-format on
}