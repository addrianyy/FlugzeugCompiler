#include <Flugzeug/Core/Iterator.hpp>
#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsolePrinter.hpp>
#include <Flugzeug/IR/Context.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionInserter.hpp>

#include <Flugzeug/Passes/CFGSimplification.hpp>
#include <Flugzeug/Passes/ConstPropagation.hpp>
#include <Flugzeug/Passes/DeadBlockElimination.hpp>
#include <Flugzeug/Passes/DeadCodeElimination.hpp>
#include <Flugzeug/Passes/InstructionSimplification.hpp>
#include <Flugzeug/Passes/LocalReordering.hpp>
#include <Flugzeug/Passes/MemoryToSSA.hpp>
#include <Flugzeug/Passes/Utils/Inline.hpp>

#include <turboc/IRGenerator.hpp>
#include <turboc/Parser.hpp>

using namespace flugzeug;

static void test_validation(Context& context) {
  Type* i1 = context.get_i1_ty();
  Type* i32 = context.get_i32_ty();
  Type* i64 = context.get_i64_ty();

  const auto f = context.create_function(i32, "test", {i32});
  const auto arg = f->get_parameter(0);

  const auto entry = f->create_block();
  //  const auto a = f->create_block();
  //  const auto b = f->create_block();
  //  const auto merge = f->create_block();

  InstructionInserter inserter;
  inserter.set_insertion_block(entry);

  auto one = i32->get_one();
  auto value = inserter.add(arg, one);
  value = inserter.add(value, one);
  value = inserter.add(value, one);
  value = inserter.add(value, one);
  value = inserter.add(value, one);

  inserter.ret(value);

  ConstPropagation::run(f);
  InstructionSimplification::run(f);

  //  inserter.add(i32->get_zero(), i32->get_zero());
  //  inserter.cond_branch(i32->get_zero(), a, b);
  //
  //  inserter.set_insertion_block(a);
  //  inserter.add(i32->get_zero(), i32->get_one());
  //  inserter.branch(merge);
  //
  //  inserter.set_insertion_block(b);
  //  inserter.branch(merge);
  //
  //  inserter.set_insertion_block(merge);
  //  inserter.ret();

  f->print(true);
  f->destroy();
}

bool inline_all(Function* f) {
  std::vector<Call*> inlinable_calls;

  for (Call& call : f->instructions<Call>()) {
    if (!call.get_target()->is_extern()) {
      inlinable_calls.push_back(&call);
    }
  }

  for (Call* call : inlinable_calls) {
    flugzeug::utils::inline_call(call);
  }

  return !inlinable_calls.empty();
}

static void optimize_function(Function* f) {
  while (true) {
    bool did_something = false;

    did_something |= CFGSimplification::run(f);
    did_something |= MemoryToSSA::run(f);
    did_something |= DeadCodeElimination::run(f);
    did_something |= ConstPropagation::run(f);
    did_something |= InstructionSimplification::run(f);
    did_something |= DeadBlockElimination::run(f);
    did_something |= LocalReordering::run(f);
    did_something |= inline_all(f);

    if (!did_something) {
      f->reassign_display_indices();
      break;
    }
  }
}

int main() {
  Context context;

  //  test_validation(context);
  //  return 0;

  const auto parsed_source = turboc::Parser::parse_from_file("../Tests/xd.tc");
  const auto functions = turboc::IRGenerator::generate(&context, parsed_source);

  ConsolePrinter printer(ConsolePrinter::Variant::Colorful);

  for (const auto& [_, f] : functions) {
    if (f->is_extern()) {
      continue;
    }

    f->validate(ValidationBehaviour::ErrorsAreFatal);

    //    f->print(printer);
    //    printer.newline();

    optimize_function(f);

    f->print(printer);
    printer.newline();

    f->validate(ValidationBehaviour::ErrorsAreFatal);
  }

  printer.newline();

  for (const auto& [_, f] : functions) {
    if (f->is_extern()) {
      continue;
    }

    f->print(printer);
    printer.newline();
  }

  for (const auto& [_, f] : functions) {
    f->destroy();
  }
}