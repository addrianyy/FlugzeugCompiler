#include <Flugzeug/Core/Iterator.hpp>
#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsolePrinter.hpp>
#include <Flugzeug/IR/Context.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionInserter.hpp>

#include <Flugzeug/Passes/CFGSimplification.hpp>
#include <Flugzeug/Passes/CmpSimplification.hpp>
#include <Flugzeug/Passes/ConstPropagation.hpp>
#include <Flugzeug/Passes/DeadBlockElimination.hpp>
#include <Flugzeug/Passes/DeadCodeElimination.hpp>
#include <Flugzeug/Passes/GeneralSimplification.hpp>
#include <Flugzeug/Passes/MemoryToSSA.hpp>

#include <turboc/IRGenerator.hpp>
#include <turboc/Parser.hpp>

using namespace flugzeug;

static void test_validation(Context& context) {
  Type* i1 = context.get_i1_ty();
  Type* i32 = context.get_i32_ty();
  Type* i64 = context.get_i64_ty();

  const auto f = context.create_function(i32, "test", {});

  const auto entry = f->create_block();
  const auto a = f->create_block();
  const auto b = f->create_block();
  const auto merge = f->create_block();

  InstructionInserter inserter;
  inserter.set_insertion_block(entry);
  inserter.cond_branch(i1->get_zero(), a, b);

  inserter.set_insertion_block(a);
  auto x = inserter.add(i32->get_constant(2), i32->get_constant(3));
  inserter.branch(merge);

  inserter.set_insertion_block(b);
  auto y = inserter.add(i32->get_constant(4), i32->get_constant(5));
  inserter.branch(merge);

  inserter.set_insertion_block(merge);
  auto z = inserter.neg(y);
  auto phi1 = inserter.phi({{entry, x}, {b, y}});
  inserter.ret(i32->get_constant(1));
  inserter.ret(i64->get_constant(3));

  f->print(true);
  f->validate(ValidationBehaviour::ErrorsToStdout);
  f->destroy();
}

static void optimize_function(Function* f) {
  while (true) {
    bool did_something = false;

    did_something |= MemoryToSSA::run(f);
    did_something |= DeadCodeElimination::run(f);
    did_something |= ConstPropagation::run(f);
    did_something |= GeneralSimplification::run(f);
    did_something |= CFGSimplification::run(f);
    did_something |= CmpSimplification::run(f);
    did_something |= DeadBlockElimination::run(f);

    if (!did_something) {
      f->reassign_display_indices();
      break;
    }
  }
}

int main() {
  Context context;

  //  test_validation(context);
  
  const auto parsed_source = turboc::Parser::parse_from_file("../Tests/main.tc");
  const auto functions = turboc::IRGenerator::generate(&context, parsed_source);

  ConsolePrinter printer(ConsolePrinter::Variant::Colorful);

  for (const auto& [_, f] : functions) {
    if (f->is_extern()) {
      continue;
    }

    f->validate(ValidationBehaviour::ErrorsAreFatal);

    f->print(printer);
    printer.newline();

    optimize_function(f);

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