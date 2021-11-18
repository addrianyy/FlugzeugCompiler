#include "Parser.hpp"

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

  unreachable();
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
