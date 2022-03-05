#include <Flugzeug/Core/Iterator.hpp>
#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsolePrinter.hpp>
#include <Flugzeug/IR/Context.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
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
#include <Flugzeug/Passes/MemoryToSSA.hpp>
#include <Flugzeug/Passes/PhiMinimization.hpp>

#include <turboc/IRGenerator.hpp>
#include <turboc/Parser.hpp>

#include <filesystem>

using namespace flugzeug;

static void test_validation(Context& context) {
  Type* i1 = context.get_i1_ty();
  Type* i32 = context.get_i32_ty();
  Type* i64 = context.get_i64_ty();

  const auto m = context.create_module();
  const auto f = m->create_function(i32, "test", {});

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

  m->print();
  m->validate(ValidationBehaviour::ErrorsAreFatal);
  m->destroy();
}

static void test_minimization(Context& context) {
  Type* i32 = context.get_i32_ty();

  const auto m = context.create_module();
  const auto f = m->create_function(i32, "test", {i32, i32, i32, i32, i32});

  const auto a = f->get_parameter(0);
  const auto b = f->get_parameter(1);
  const auto c = f->get_parameter(2);
  const auto d = f->get_parameter(3);
  const auto e = f->get_parameter(4);

  const auto entry = f->create_block();
  const auto b1 = f->create_block();
  const auto b2 = f->create_block();
  const auto b3 = f->create_block();

  InstructionInserter inserter(entry);
  const auto cond = inserter.compare_sgte(a, i32->get_one());
  inserter.cond_branch(cond, b1, b2);

  inserter.set_insertion_block(b1);
  const auto phi1 = inserter.phi(i32);
  inserter.branch(b2);

  inserter.set_insertion_block(b2);
  const auto phi2 = inserter.phi(i32);
  inserter.cond_branch(cond, b1, b3);

  inserter.set_insertion_block(b3);
  inserter.ret(i32->get_one());

  phi1->add_incoming(entry, a);
  phi1->add_incoming(b2, phi2);

  phi2->add_incoming(entry, a);
  phi2->add_incoming(b1, phi1);

  PhiMinimization::run(f);

  m->print();
  m->destroy();
}

#include <Flugzeug/Passes/Analysis/PointerAliasing.hpp>

static void optimize_function(Function* f) {
  while (true) {
    bool did_something = false;

    did_something |= CallInlining::run(f, InliningStrategy::InlineEverything);
    did_something |= CFGSimplification::run(f);
    did_something |= MemoryToSSA::run(f);
    did_something |= PhiMinimization::run(f);
    did_something |= DeadCodeElimination::run(f);
    did_something |= ConstPropagation::run(f);
    did_something |= InstructionSimplification::run(f);
    did_something |= DeadBlockElimination::run(f);
    did_something |= LocalReordering::run(f);
    did_something |= LoopUnrolling::run(f);
    did_something |= LoopInvariantOptimization::run(f);
    did_something |= BlockInvariantPropagation::run(f);
    did_something |= ConditionalFlattening::run(f);
    did_something |= KnownBitsOptimization::run(f);
    did_something |= InstructionDeduplication::run(f, DeduplicationStrategy::BlockLocal);

    if (!did_something) {
      f->reassign_display_indices();

      {
        log_debug("{}", f->get_name());
        analysis::PointerAliasing aliasing(f);
        aliasing.debug_dump();
        log_debug("");
      }

      break;
    }
  }
}

static void debug_print_loops(Function* f) {
  const auto loops = analysis::analyze_function_loops(f);

  log_debug("{}:", f->get_name());
  for (const auto& loop : loops) {
    loop->debug_print("  ");
  }
  log_debug("");
}

int main() {
  std::filesystem::current_path("../");
  std::filesystem::remove_all("Graphs/");
  std::filesystem::create_directory("Graphs/");

  Context context;

  const auto printing_method = IRPrintingMethod::Standard;

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