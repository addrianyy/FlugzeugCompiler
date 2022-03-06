#include <Flugzeug/Core/Iterator.hpp>
#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsolePrinter.hpp>
#include <Flugzeug/IR/Context.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/FilePrinter.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionInserter.hpp>
#include <Flugzeug/IR/Module.hpp>

#include <Flugzeug/Passes/Analysis/Loops.hpp>
#include <Flugzeug/Passes/BlockInvariantPropagation.hpp>
#include <Flugzeug/Passes/CFGSimplification.hpp>
#include <Flugzeug/Passes/CallInlining.hpp>
#include <Flugzeug/Passes/ConditionalFlattening.hpp>
#include <Flugzeug/Passes/ConstPropagation.hpp>
#include <Flugzeug/Passes/DeadBlockElimination.hpp>
#include <Flugzeug/Passes/DeadCodeElimination.hpp>
#include <Flugzeug/Passes/DeadStoreElimination.hpp>
#include <Flugzeug/Passes/InstructionDeduplication.hpp>
#include <Flugzeug/Passes/InstructionSimplification.hpp>
#include <Flugzeug/Passes/KnownBitsOptimization.hpp>
#include <Flugzeug/Passes/KnownLoadElimination.hpp>
#include <Flugzeug/Passes/LocalReordering.hpp>
#include <Flugzeug/Passes/LoopInvariantOptimization.hpp>
#include <Flugzeug/Passes/LoopUnrolling.hpp>
#include <Flugzeug/Passes/MemoryToSSA.hpp>
#include <Flugzeug/Passes/PhiMinimization.hpp>

#include <turboc/IRGenerator.hpp>
#include <turboc/Parser.hpp>

#include <filesystem>

using namespace flugzeug;

static void optimize_function(Function* f) {
  while (true) {
    bool did_something = false;

    did_something |= opt::CallInlining::run(f, opt::InliningStrategy::InlineEverything);
    did_something |= opt::CFGSimplification::run(f);
    did_something |= opt::MemoryToSSA::run(f);
    did_something |= opt::PhiMinimization::run(f);
    did_something |= opt::DeadCodeElimination::run(f);
    did_something |= opt::ConstPropagation::run(f);
    did_something |= opt::InstructionSimplification::run(f);
    did_something |= opt::DeadBlockElimination::run(f);
    did_something |= opt::LocalReordering::run(f);
    did_something |= opt::LoopUnrolling::run(f);
    did_something |= opt::LoopInvariantOptimization::run(f);
    did_something |= opt::BlockInvariantPropagation::run(f);
    did_something |= opt::ConditionalFlattening::run(f);
    did_something |= opt::KnownBitsOptimization::run(f);
    did_something |= opt::InstructionDeduplication::run(f, opt::OptimizationLocality::BlockLocal);
    did_something |= opt::KnownLoadElimination::run(f, opt::OptimizationLocality::BlockLocal);
    did_something |= opt::DeadStoreElimination::run(f, opt::OptimizationLocality::BlockLocal);

    if (!did_something) {
      f->reassign_display_indices();
      break;
    }
  }
}

int main() {
  std::filesystem::current_path("../");
  std::filesystem::remove_all("Graphs/");
  std::filesystem::create_directory("Graphs/");

  Context context;

  const auto printing_method = IRPrintingMethod::Compact;

  const auto parsed_source = turboc::Parser::parse_from_file("Tests/memory.tc");
  const auto module = turboc::IRGenerator::generate(&context, parsed_source);

  for (Function& f : module->local_functions()) {
    optimize_function(&f);

    f.generate_graph(fmt::format("Graphs/{}.svg", f.get_name()), printing_method);
  }

  module->validate(ValidationBehaviour::ErrorsAreFatal);
  module->print(printing_method);
  module->destroy();
}