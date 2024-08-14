#include "Parser.hpp"
#include "Conversion.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace turboc;

Type Parser::parse_type() {
  const auto kind = conv::keyword_to_type_kind(lexer.consume_keyword());
  verify(kind, "Encountered invalid type token.");

  uint32_t indirection = 0;

  while (true) {
    if (!lexer.current_token().is(Token::Kind::Mul)) {
      break;
    }

    lexer.consume_token();

    indirection++;
  }

  return Type(*kind, indirection);
}

std::unique_ptr<Expr> Parser::parse_binary_expression(int32_t min_precedence,
                                                      std::unique_ptr<Expr> expr) {
  const auto get_token_precedence = [&]() {
    if (const auto op = conv::token_to_binary_op(lexer.current_token().get_kind())) {
      return get_binary_op_precedence(*op);
    } else {
      return -1;
    }
  };

  while (true) {
    const auto precedence = get_token_precedence();
    if (precedence < min_precedence) {
      return expr;
    }

    const auto op = *conv::token_to_binary_op(lexer.consume_token().get_kind());
    auto right = parse_primary_expression();

    const auto next_precedence = get_token_precedence();
    if (next_precedence > precedence) {
      right = parse_binary_expression(precedence + 1, std::move(right));
    }

    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
}

std::unique_ptr<Expr> Parser::parse_unary_expression(UnaryOp op) {
  lexer.consume_token();
  return std::make_unique<UnaryExpr>(op, parse_primary_expression());
}

std::unique_ptr<Expr> Parser::parse_call_expression(const std::string& function) {
  std::vector<std::unique_ptr<Expr>> arguments;

  parse_argument_list([&]() { arguments.push_back(parse_expression()); });

  return std::make_unique<CallExpr>(function, std::move(arguments));
}

std::unique_ptr<Expr> Parser::parse_number_expression() {
  const auto literal = lexer.current_token().get_number_literal();
  lexer.consume_token();

  Type::Kind type;

  if (literal.type_override) {
    type = conv::type_override_to_type(*literal.type_override);
  } else {
    const bool is_signed = literal.base == 10;
    bool big_number;

    type = is_signed ? Type::Kind::I32 : Type::Kind::U32;

    if (is_signed) {
      using Lim = std::numeric_limits<int32_t>;
      big_number = int64_t(literal.literal) > int64_t(Lim::max()) ||
                   int64_t(literal.literal) < int64_t(Lim::min());
    } else {
      using Lim = std::numeric_limits<uint32_t>;
      big_number = uint64_t(literal.literal) > uint64_t(Lim::max()) ||
                   uint64_t(literal.literal) < uint64_t(Lim::min());
    }

    if (big_number) {
      type = is_signed ? Type::Kind::I64 : Type::Kind::U64;
    }
  }

  return std::make_unique<NumberExpr>(Type(type), literal.literal);
}

std::unique_ptr<Expr> Parser::parse_primary_expression() {
  if (const auto op = conv::token_to_unary_op(lexer.current_token().get_kind())) {
    return parse_unary_expression(*op);
  }

  std::unique_ptr<Expr> expr;

  const auto kind = lexer.current_token().get_kind();
  if (kind == Token::Kind::Identifier) {
    const auto identifier = std::string(lexer.current_token().get_identifier());
    lexer.consume_token();

    if (lexer.current_token().is(Token::Kind::ParenOpen)) {
      expr = parse_call_expression(identifier);
    } else {
      expr = std::make_unique<VariableExpr>(identifier);
    }
  } else if (kind == Token::Kind::NumberLiteral) {
    expr = parse_number_expression();
  } else if (kind == Token::Kind::ParenOpen) {
    lexer.consume_token();

    const auto& token = lexer.current_token();
    if (token.is_keyword() && conv::keyword_to_type_kind(token.get_keyword())) {
      const auto type = parse_type();

      lexer.consume_expect(Token::Kind::ParenClose);

      expr = std::make_unique<CastExpr>(parse_primary_expression(), type);
    } else {
      lexer.restore(1);
      expr = parse_paren_expression();
    }
  } else {
    fatal_error("Unexpected token in primary expression {}.", lexer.current_token().format());
  }

  if (lexer.current_token().is(Token::Kind::BracketOpen)) {
    lexer.consume_token();
    auto index = parse_expression();
    lexer.consume_expect(Token::Kind::BracketClose);

    expr = std::make_unique<ArrayExpr>(std::move(expr), std::move(index));
  }

  return expr;
}

std::unique_ptr<Expr> Parser::parse_paren_expression() {
  lexer.consume_expect(Token::Kind::ParenOpen);
  auto expr = parse_expression();
  lexer.consume_expect(Token::Kind::ParenClose);

  return expr;
}

std::unique_ptr<Expr> Parser::parse_expression() {
  auto left = parse_primary_expression();
  return parse_binary_expression(0, std::move(left));
}

std::unique_ptr<Stmt> Parser::parse_expression_statement() {
  auto expr = parse_expression();

  if (lexer.current_token().is(Token::Kind::Assign)) {
    lexer.consume_token();
    return std::make_unique<AssignStmt>(std::move(expr), parse_expression());
  } else if (const auto op = conv::binary_op_for_binary_assign(lexer.current_token().get_kind())) {
    lexer.consume_token();
    return std::make_unique<BinaryAssignStmt>(std::move(expr), *op, parse_expression());
  }

  return expr;
}

std::unique_ptr<Stmt> Parser::parse_declaration() {
  const auto declaration_type = parse_type();
  const auto name = std::string(lexer.consume_identifier());

  auto type = declaration_type;

  std::unique_ptr<Expr> value;
  std::unique_ptr<Expr> array_size;

  if (lexer.current_token().is(Token::Kind::BracketOpen)) {
    lexer.consume_token();

    array_size = parse_expression();
    type = Type(declaration_type.get_kind(), declaration_type.get_indirection() + 1);

    lexer.consume_expect(Token::Kind::BracketClose);
  }

  if (lexer.current_token().is(Token::Kind::Assign)) {
    lexer.consume_token();
    value = parse_expression();
  }

  return std::make_unique<DeclareStmt>(type, declaration_type, name, std::move(value),
                                       std::move(array_size));
}

std::unique_ptr<Stmt> Parser::parse_if() {
  lexer.consume_expect(Token::Keyword::If);

  std::vector<IfStmt::Arm> arms;
  std::unique_ptr<BodyStmt> default_body;

  {
    auto condition = parse_paren_expression();
    auto body = parse_body();

    arms.emplace_back(std::move(condition), std::move(body));
  }

  while (true) {
    if (!lexer.current_token().is_keyword(Token::Keyword::Else)) {
      break;
    }
    lexer.consume_token();

    std::unique_ptr<Expr> condition;

    if (lexer.current_token().is_keyword(Token::Keyword::If)) {
      lexer.consume_token();
      condition = parse_paren_expression();
    }

    auto body = parse_body();

    if (condition) {
      arms.emplace_back(std::move(condition), std::move(body));
    } else {
      default_body = std::move(body);
      break;
    }
  }

  return std::make_unique<IfStmt>(std::move(arms), std::move(default_body));
}

std::unique_ptr<Stmt> Parser::parse_for() {
  lexer.consume_expect(Token::Keyword::For);
  lexer.consume_expect(Token::Kind::ParenOpen);

  std::unique_ptr<Stmt> init;
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Stmt> step;

  if (!lexer.current_token().is(Token::Kind::Semicolon)) {
    init = parse_statement();
  }
  lexer.consume_expect(Token::Kind::Semicolon);

  if (!lexer.current_token().is(Token::Kind::Semicolon)) {
    condition = parse_expression();
  }
  lexer.consume_expect(Token::Kind::Semicolon);

  if (!lexer.current_token().is(Token::Kind::Semicolon)) {
    step = parse_statement();
  }
  lexer.consume_expect(Token::Kind::ParenClose);

  return std::make_unique<ForStmt>(std::move(init), std::move(condition), std::move(step),
                                   parse_body());
}

std::unique_ptr<Stmt> Parser::parse_statement() {
  const auto& token = lexer.current_token();
  if (token.is_keyword()) {
    const auto keyword = token.get_keyword();

    if (conv::keyword_to_type_kind(keyword)) {
      return parse_declaration();
    }

    switch (keyword) {
      case Token::Keyword::If:
        return parse_if();
      case Token::Keyword::For:
        return parse_for();
      case Token::Keyword::Return: {
        lexer.consume_token();

        auto return_value =
          lexer.current_token().is(Token::Kind::Semicolon) ? nullptr : parse_expression();

        return std::make_unique<ReturnStmt>(std::move(return_value));
      }
      case Token::Keyword::While: {
        lexer.consume_token();

        auto condition = parse_paren_expression();
        auto body = parse_body();

        return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
      }
      case Token::Keyword::Continue:
        lexer.consume_token();
        return std::make_unique<ContinueStmt>();
      case Token::Keyword::Break:
        lexer.consume_token();
        return std::make_unique<BreakStmt>();
      default:
        unreachable();
    }
  } else {
    return parse_expression_statement();
  }
}

std::unique_ptr<BodyStmt> Parser::parse_body() {
  std::vector<std::unique_ptr<Stmt>> stmts;

  lexer.consume_expect(Token::Kind::BraceOpen);

  while (!lexer.current_token().is(Token::Kind::BraceClose)) {
    auto stmt = parse_statement();

    const auto kind = stmt->kind();
    const auto no_semicolon =
      kind == Stmt::Kind::While || kind == Stmt::Kind::If || kind == Stmt::Kind::For;

    if (!no_semicolon) {
      lexer.consume_expect(Token::Kind::Semicolon);
    }

    stmts.push_back(std::move(stmt));
  }

  lexer.consume_expect(Token::Kind::BraceClose);

  return std::make_unique<BodyStmt>(std::move(stmts));
}

FunctionPrototype Parser::parse_prototype() {
  const auto return_type = parse_type();
  const auto name = std::string(lexer.consume_identifier());

  std::vector<FunctionPrototype::Argument> arguments;

  parse_argument_list([&]() {
    const auto arg_type = parse_type();
    const auto arg_name = std::string(lexer.consume_identifier());

    arguments.emplace_back(arg_type, arg_name);
  });

  return {name, arguments, return_type};
}

Function Parser::parse_function() {
  const bool is_extern = lexer.current_token().is_keyword(Token::Keyword::Extern);
  if (is_extern) {
    lexer.consume_token();
  }

  const auto prototype = parse_prototype();

  std::unique_ptr<BodyStmt> body;
  if (is_extern) {
    lexer.consume_expect(Token::Kind::Semicolon);
  } else {
    body = parse_body();
  }

  return {prototype, std::move(body)};
}

std::vector<Function> Parser::parse_functions() {
  std::vector<Function> functions;

  while (!lexer.current_token().is(Token::Kind::Eof)) {
    functions.push_back(parse_function());
  }

  return functions;
}

std::vector<Function> Parser::parse(turboc::Lexer& lexer) {
  Parser parser(lexer);
  return parser.parse_functions();
}

std::vector<Function> turboc::Parser::parse_from_file(const std::string& source_path) {
  Lexer lexer = Lexer::from_file(source_path);
  Parser parser(lexer);
  return parser.parse_functions();
}