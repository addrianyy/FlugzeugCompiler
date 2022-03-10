#include "ModuleFromASTGenerator.hpp"

using namespace flugzeug;

Type* ModuleFromASTGenerator::convert_type(PRType type) {
  Type* result = nullptr;

  // clang-format off
  switch (type.base_kw) {
    case Token::Keyword::I1: result = context->get_i1_ty(); break;
    case Token::Keyword::I8: result = context->get_i8_ty(); break;
    case Token::Keyword::I16: result = context->get_i16_ty(); break;
    case Token::Keyword::I32: result = context->get_i32_ty(); break;
    case Token::Keyword::I64: result = context->get_i64_ty(); break;
    case Token::Keyword::Void: result = context->get_void_ty(); break;
    default: unreachable();
  }
  // clang-format on

  if (type.indirection > 0) {
    if (result->is_void()) {
      result = context->get_i8_ty();
    }

    result = result->ref(type.indirection);
  }

  return result;
}

ModuleFromASTGenerator::ModuleFromASTGenerator(
  Module* module, std::vector<std::unique_ptr<PRFunction>> parsed_functions)
    : context(module->get_context()), module(module),
      parsed_functions(std::move(parsed_functions)) {
  verify(module->is_empty(), "Module passed to ModuleFromASTGenerator must be empty");
}

void ModuleFromASTGenerator::generate() {
  if (parsed_functions.empty()) {
    return;
  }

  for (auto& function : parsed_functions) {
    std::vector<Type*> parameter_types;
    parameter_types.reserve(function->parameters.size());

    for (auto& parameter : function->parameters) {
      parameter_types.push_back(convert_type(parameter.type));
    }

    module->create_function(convert_type(function->return_type), std::string(function->name),
                            parameter_types);

    if (!function->is_extern) {
      verify(!function->blocks.empty(), "Non-extern functions must contain blocks");
    }
  }
}
