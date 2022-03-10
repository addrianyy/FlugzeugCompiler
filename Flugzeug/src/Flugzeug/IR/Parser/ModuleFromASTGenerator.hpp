#pragma once
#include "AST.hpp"
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>
#include <Flugzeug/IR/Module.hpp>

namespace flugzeug {

class ModuleFromASTGenerator {
  Context* context;
  Module* module;

  std::vector<std::unique_ptr<PRFunction>> parsed_functions;

  Type* convert_type(PRType type);

public:
  ModuleFromASTGenerator(Module* module, std::vector<std::unique_ptr<PRFunction>> parsed_functions);

  void generate();
};

} // namespace flugzeug