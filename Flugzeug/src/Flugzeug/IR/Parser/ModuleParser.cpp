#include "ModuleParser.hpp"
#include "Lexer.hpp"
#include "ModuleFromASTGenerator.hpp"
#include "Parser.hpp"

void flugzeug::parse_to_module(std::string source, Module* module) {
  Lexer lexer(std::move(source));
  Parser parser(lexer);
  ModuleFromASTGenerator generator(module, parser.parse());
  generator.generate();
}