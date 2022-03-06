#include "Compiler.hpp"
#include "IRGenerator.hpp"
#include "Parser.hpp"

using namespace flugzeug;

Module* turboc::Compiler::compile_from_file(Context* context, const std::string& source_path) {
  const auto parsed_source = turboc::Parser::parse_from_file(source_path);
  return turboc::IRGenerator::generate(context, parsed_source);
}