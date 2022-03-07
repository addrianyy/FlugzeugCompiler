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
#include <Flugzeug/Passes/InstructionDeduplication.hpp>
#include <Flugzeug/Passes/InstructionSimplification.hpp>
#include <Flugzeug/Passes/KnownBitsOptimization.hpp>
#include <Flugzeug/Passes/LocalReordering.hpp>
#include <Flugzeug/Passes/LoopInvariantOptimization.hpp>
#include <Flugzeug/Passes/LoopUnrolling.hpp>
#include <Flugzeug/Passes/MemoryOptimization.hpp>
#include <Flugzeug/Passes/MemoryToSSA.hpp>
#include <Flugzeug/Passes/PhiMinimization.hpp>

#include <Flugzeug/Passes/Analysis/Paths.hpp>

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

static void test(Context* context) {
  const auto module = context->create_module();
  const auto function = module->create_function(context->get_void_ty(), "main", {});

  InstructionInserter ins(function->create_block());

  const auto a = function->create_block();
  const auto b = function->create_block();
  const auto c = function->create_block();
  const auto d = function->create_block();
  const auto e = function->create_block();
  const auto f = function->create_block();
  const auto g = function->create_block();
  const auto h = function->create_block();
  const auto i = function->create_block();
  const auto cond = context->get_i1_ty()->get_undef();

  ins.branch(a);

  ins.set_insertion_block(a);
  ins.cond_branch(cond, b, c);

  ins.set_insertion_block(c);
  ins.branch(d);

  ins.set_insertion_block(d);
  ins.cond_branch(cond, e, f);

  ins.set_insertion_block(e);
  ins.cond_branch(cond, d, g);

  ins.set_insertion_block(f);
  ins.cond_branch(cond, h, i);

  ins.set_insertion_block(i);
  ins.branch(a);

  for (Function& function : module->local_functions()) {
    function.generate_graph(fmt::format("Graphs/{}.svg", function.get_name()),
                            IRPrintingMethod::Standard);
  }

  {
    const auto x = analysis::get_blocks_inbetween(d, d, a);
    for (const auto& w : x) {
      log_debug("{}", w->format());
    }
  }

  {
    const auto x = analysis::get_blocks_from_dominator_to_target(a, e);
    for (const auto& w : x) {
      log_info("{}", w->format());
    }
  }

  {
    const auto x = analysis::get_blocks_from_dominator_to_target(c, i);
    for (const auto& w : x) {
      log_warn("{}", w->format());
    }
  }

  module->destroy();
}

static void optimize_function(Function* f) {
  constexpr bool enable_loop_optimizations = false;

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
    if (enable_loop_optimizations) {
      did_something |= opt::LoopUnrolling::run(f);
      did_something |= opt::LoopInvariantOptimization::run(f);
    }
    did_something |= opt::BlockInvariantPropagation::run(f);
    did_something |= opt::ConditionalFlattening::run(f);
    did_something |= opt::KnownBitsOptimization::run(f);
    did_something |= opt::InstructionDeduplication::run(f, opt::OptimizationLocality::BlockLocal);
    did_something |= opt::MemoryOptimization::run(f, opt::OptimizationLocality::BlockLocal);

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
  std::filesystem::remove_all("TestResults/");
  std::filesystem::create_directory("TestResults/");

  Context context;
  test(&context);
  return 1;

  const auto printing_method = IRPrintingMethod::Compact;
  const auto source_path = "TestsBF/mandel.bf";

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

  if (false) {
    for (Function& f : module->local_functions()) {
      f.generate_graph(fmt::format("Graphs/{}.svg", f.get_name()), printing_method);
    }
  }

  FilePrinter file_printer("TestResults/result.flug");
  ConsolePrinter console_printer(ConsolePrinter::Variant::ColorfulIfSupported);

  module->validate(ValidationBehaviour::ErrorsAreFatal);
  module->print(file_printer, printing_method);
  module->destroy();
}