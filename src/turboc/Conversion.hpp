#pragma once
#include "AST.hpp"

namespace turboc::conv {
std::optional<Type::Kind> keyword_to_type_kind(Token::Keyword keyword);
std::optional<UnaryOp> token_to_unary_op(Token::Kind kind);
std::optional<BinaryOp> token_to_binary_op(Token::Kind kind);
std::optional<BinaryOp> binary_op_for_binary_assign(Token::Kind kind);
Type::Kind type_override_to_type(Token::TypeOverride override);
} // namespace turboc::conv