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

static void print_loop(const std::string& indentation, const Loop& loop) {
  std::string blocks;
  std::string blocks_no_subloops;
  std::string back_edges_from;
  std::string exiting_edges;

  for (Block* block : loop.blocks) {
    blocks += block->format() + ", ";
  }

  for (Block* block : loop.blocks_without_subloops) {
    blocks_no_subloops += block->format() + ", ";
  }

  for (Block* block : loop.back_edges_from) {
    back_edges_from += block->format() + ", ";
  }

  for (const auto [from, to] : loop.exiting_edges) {
    exiting_edges += fmt::format("({} -> {}), ", from->format(), to->format());
  }

  const auto remove_comma = [](std::string& s) {
    if (s.ends_with(", ")) {
      s = s.substr(0, s.size() - 2);
    }
  };

  remove_comma(blocks);
  remove_comma(blocks_no_subloops);
  remove_comma(back_edges_from);
  remove_comma(exiting_edges);

  log_debug("{}Loop", indentation);
  log_debug("{}  header: {}", indentation, loop.header->format());
  log_debug("{}  blocks: {}", indentation, blocks);
  log_debug("{}  blocks (no sub-loops): {}", indentation, blocks_no_subloops);
  log_debug("{}  back edges from: {}", indentation, back_edges_from);
  log_debug("{}  exiting edges: {}", indentation, exiting_edges);

  if (!loop.sub_loops.empty()) {
    log_debug("{}  sub loops:", indentation);

    for (const auto& sub_loop : loop.sub_loops) {
      print_loop(indentation + "    ", *sub_loop);
    }
  }
}

static void analyze_loops(Function* f) {
  const auto loops = analyze_function_loops(f);

  log_debug("{}:", f->get_name());

  for (const auto& loop : loops) {
    print_loop("  ", *loop);
  }
  log_debug("");
}

int main() {
  Context context;
  ConsolePrinter printer(ConsolePrinter::Variant::Colorful);

  const auto parsed_source = turboc::Parser::parse_from_file("../Tests/main.tc");
  const auto module = turboc::IRGenerator::generate(&context, parsed_source);

  for (const Function& f : *module) {
    //    show_calls(&f);
  }

  for (Function& f : module->local_functions()) {
    f.validate(ValidationBehaviour::ErrorsAreFatal);

    optimize_function(&f);
    //    analyze_loops(&f);

    f.validate(ValidationBehaviour::ErrorsAreFatal);

    f.generate_graph(fmt::format("../Graphs/{}.svg", f.get_name()));
  }

  module->destroy();
}