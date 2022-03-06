#include "Compiler.hpp"

#include <fstream>

#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionInserter.hpp>

using namespace flugzeug;

Module* bf::Compiler::compile_from_file(Context* context, const std::string& source_path) {
  std::string source;
  {
    std::ifstream file(source_path);
    verify(!!file, "Failed to open `{}` for reading", source_path);

    file.seekg(0, std::ios::end);
    source.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    source.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  }

  const auto void_ty = context->get_void_ty();
  const auto i8 = context->get_i8_ty();
  const auto i8_ptr = i8->ref();
  const auto i64 = context->get_i64_ty();

  const auto module = context->create_module();
  const auto get_char = module->create_function(i8, "get_char", {});
  const auto put_char = module->create_function(void_ty, "put_char", {i8});
  const auto main_func = module->create_function(void_ty, "main", {i8_ptr});

  InstructionInserter ins(main_func->create_block());

  const auto buffer = main_func->get_parameter(0);
  const auto index = ins.stack_alloc(i64);
  ins.store(index, i64->get_zero());

  const auto get_pointer = [&]() { return ins.offset(buffer, ins.load(index)); };

  struct Loop {
    Block* header;
    Block* body;
    Block* after;
  };
  std::vector<Loop> loops;

  for (char c : source) {
    switch (c) {
    case '<':
    case '>': {
      const auto amount = i64->get_constant(c == '>' ? 1 : -1);
      ins.store(index, ins.add(ins.load(index), amount));
      break;
    }

    case '+':
    case '-': {
      const auto pointer = get_pointer();
      const auto amount = i8->get_constant(c == '+' ? 1 : -1);
      ins.store(pointer, ins.add(ins.load(pointer), amount));
      break;
    }

    case ',': {
      ins.store(get_pointer(), ins.call(get_char, {}));
      break;
    }

    case '.': {
      ins.call(put_char, {ins.load(get_pointer())});
      break;
    }

    case '[': {
      const auto header = main_func->create_block();
      const auto body = main_func->create_block();
      const auto after = main_func->create_block();

      ins.branch(header);

      ins.set_insertion_block(header);
      ins.cond_branch(ins.compare_ne(ins.load(get_pointer()), i8->get_zero()), body, after);

      ins.set_insertion_block(body);

      loops.push_back({header, body, after});

      break;
    }

    case ']': {
      const auto loop = loops.back();
      loops.pop_back();

      ins.branch(loop.after);
      ins.set_insertion_block(loop.after);

      break;
    }

    default:
      break;
    }
  }

  ins.ret();

  module->validate(ValidationBehaviour::ErrorsAreFatal);

  return module;
}
