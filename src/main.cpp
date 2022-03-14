#include <Flugzeug/Core/Iterator.hpp>
#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsolePrinter.hpp>
#include <Flugzeug/IR/Context.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/FilePrinter.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Module.hpp>

#include <Flugzeug/Passes/BlockInvariantPropagation.hpp>
#include <Flugzeug/Passes/CFGSimplification.hpp>
#include <Flugzeug/Passes/CallInlining.hpp>
#include <Flugzeug/Passes/ConditionalFlattening.hpp>
#include <Flugzeug/Passes/ConstPropagation.hpp>
#include <Flugzeug/Passes/DeadBlockElimination.hpp>
#include <Flugzeug/Passes/DeadCodeElimination.hpp>
#include <Flugzeug/Passes/GlobalReordering.hpp>
#include <Flugzeug/Passes/InstructionDeduplication.hpp>
#include <Flugzeug/Passes/InstructionSimplification.hpp>
#include <Flugzeug/Passes/KnownBitsOptimization.hpp>
#include <Flugzeug/Passes/LocalReordering.hpp>
#include <Flugzeug/Passes/LoopInvariantOptimization.hpp>
#include <Flugzeug/Passes/LoopMemoryExtraction.hpp>
#include <Flugzeug/Passes/LoopRotation.hpp>
#include <Flugzeug/Passes/LoopUnrolling.hpp>
#include <Flugzeug/Passes/MemoryOptimization.hpp>
#include <Flugzeug/Passes/MemoryToSSA.hpp>
#include <Flugzeug/Passes/PassRunner.hpp>
#include <Flugzeug/Passes/PhiMinimization.hpp>

#include <bf/Compiler.hpp>
#include <turboc/Compiler.hpp>

#include <filesystem>

using namespace flugzeug;

static Module* compile_source(Context* context, const std::string& source_path) {
  if (source_path.ends_with(".tc")) {
    return turboc::Compiler::compile_from_file(context, source_path);
  } else if (source_path.ends_with(".bf")) {
    return bf::Compiler::compile_from_file(context, source_path);
  } else {
    fatal_error("Unknown source file extension.");
  }
}

static void optimize_function(Function* f) {
  constexpr bool enable_loop_optimizations = true;

  while (true) {
    FunctionPassRunner runner(f);

    runner.run<opt::CallInlining>(opt::InliningStrategy::InlineEverything);
    runner.run<opt::CFGSimplification>();
    runner.run<opt::MemoryToSSA>();
    runner.run<opt::PhiMinimization>();
    runner.run<opt::DeadCodeElimination>();
    runner.run<opt::ConstPropagation>();
    runner.run<opt::InstructionSimplification>();
    runner.run<opt::DeadBlockElimination>();
    runner.run<opt::LocalReordering>();
    if (enable_loop_optimizations) {
      runner.run<opt::LoopRotation>();
      runner.run<opt::LoopUnrolling>();
      runner.run<opt::LoopInvariantOptimization>();
      runner.run<opt::LoopMemoryExtraction>();
      runner.run<opt::CFGSimplification>();
    }
    runner.run<opt::BlockInvariantPropagation>();
    runner.run<opt::ConditionalFlattening>();
    runner.run<opt::KnownBitsOptimization>();
    runner.run<opt::InstructionDeduplication>(opt::OptimizationLocality::Global);
    runner.run<opt::MemoryOptimization>(opt::OptimizationLocality::Global);
    runner.run<opt::GlobalReordering>();

    if (!runner.did_something()) {
      f->reassign_display_indices();
      break;
    }
  }
}

int main() {
  std::filesystem::current_path("../");
  std::filesystem::remove_all("Graphs/");
  std::filesystem::create_directory("Graphs/");
  std::filesystem::remove_all("TestResults/");
  std::filesystem::create_directory("TestResults/");

  Context context;

  const auto printing_method = IRPrintingMethod::Standard;
  const auto source_path = "TestsTC/loops.tc";
  //  const auto source_path = "TestsBF/test.bf";

  const auto module = compile_source(&context, source_path);

  if (true) {
    const auto start = std::chrono::high_resolution_clock::now();
    for (Function& f : module->local_functions()) {
      optimize_function(&f);
    }
    const auto end = std::chrono::high_resolution_clock::now();

    log_info("Optimized module in {}ms.",
             std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  }

  if (true) {
    for (Function& f : module->local_functions()) {
      f.generate_graph(fmt::format("Graphs/{}.svg", f.get_name()), printing_method);
    }
  }

  FilePrinter file_printer("TestResults/result.flug");
  ConsolePrinter console_printer(ConsolePrinter::Variant::ColorfulIfSupported);

  module->validate(ValidationBehaviour::ErrorsAreFatal);
  module->print(console_printer, printing_method);
  module->destroy();
}