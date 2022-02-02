#include <Flugzeug/Core/Iterator.hpp>
#include <Flugzeug/Core/Log.hpp>

#include <Flugzeug/IR/ConsolePrinter.hpp>
#include <Flugzeug/IR/Context.hpp>
#include <Flugzeug/IR/DominatorTree.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionInserter.hpp>
#include <Flugzeug/IR/Module.hpp>

#include <Flugzeug/Passes/Analysis/Loops.hpp>
#include <Flugzeug/Passes/CFGSimplification.hpp>
#include <Flugzeug/Passes/ConstPropagation.hpp>
#include <Flugzeug/Passes/DeadBlockElimination.hpp>
#include <Flugzeug/Passes/DeadCodeElimination.hpp>
#include <Flugzeug/Passes/InstructionSimplification.hpp>
#include <Flugzeug/Passes/LocalReordering.hpp>
#include <Flugzeug/Passes/LoopUnrolling.hpp>
#include <Flugzeug/Passes/MemoryToSSA.hpp>
#include <Flugzeug/Passes/Utils/Inline.hpp>

#include <turboc/IRGenerator.hpp>
#include <turboc/Parser.hpp>

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

  f->print();
  f->validate(ValidationBehaviour::ErrorsToStdout);
  m->destroy();
}

static bool inline_all(Function* f) {
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

    did_something |= inline_all(f);
    did_something |= CFGSimplification::run(f);
    did_something |= MemoryToSSA::run(f);
    did_something |= DeadCodeElimination::run(f);
    did_something |= ConstPropagation::run(f);
    did_something |= InstructionSimplification::run(f);
    did_something |= DeadBlockElimination::run(f);
    did_something |= LocalReordering::run(f);
    did_something |= LoopUnrolling::run(f);

    if (!did_something) {
      f->reassign_display_indices();
      break;
    }
  }
}

static void debug_print_loops(Function* f) {
  const auto loops = analyze_function_loops(f);

  log_debug("{}:", f->get_name());
  for (const auto& loop : loops) {
    loop->debug_print("  ");
  }
  log_debug("");
}

int main() {
  Context context;
  ConsolePrinter printer(ConsolePrinter::Variant::Colorful);

  const auto parsed_source = turboc::Parser::parse_from_file("../Tests/main.tc");
  const auto module = turboc::IRGenerator::generate(&context, parsed_source);

  for (Function& f : module->local_functions()) {
    f.validate(ValidationBehaviour::ErrorsAreFatal);

    optimize_function(&f);
    //    debug_print_loops(&f);

    f.validate(ValidationBehaviour::ErrorsAreFatal);

    f.generate_graph(fmt::format("../Graphs/{}.svg", f.get_name()));
  }

  module->destroy();
}