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

  const auto parsed_source = turboc::Parser::parse_from_file("../Tests/main.tc");
  const auto functions = turboc::IRGenerator::generate(&context, parsed_source);

  ConsolePrinter printer(ConsolePrinter::Variant::Colorful);

  for (const auto& [_, f] : functions) {
    if (f->is_extern()) {
      continue;
    }

    f->print(printer);
    printer.newline();

    optimize_function(f);
  }

  printer.newline();

  for (const auto& [_, f] : functions) {
    if (f->is_extern()) {
      continue;
    }

    DominatorTree tree(f);

    f->print(printer);
    printer.newline();
  }

  for (const auto& [_, f] : functions) {
    f->destroy();
  }
}