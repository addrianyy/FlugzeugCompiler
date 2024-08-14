#include "GraphDumper.hpp"
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

#include <Flugzeug/Core/Environment.hpp>
#include <Flugzeug/Core/Process.hpp>

#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace flugzeug;

namespace colors {

constexpr std::string_view keyword = "5C962C";
constexpr std::string_view value = "A68A0D";
constexpr std::string_view constant = "BBBBBB";
constexpr std::string_view type = "3993D4";
constexpr std::string_view block = "808080";

constexpr std::string_view default_text = "BBBBBB";
constexpr std::string_view background = "2B2B2B";
constexpr std::string_view box_border = "BBBBBB";

}  // namespace colors

static void replace_occurrences(std::string& string, std::string_view from, std::string_view to) {
  size_t start_pos = 0;
  while ((start_pos = string.find(from, start_pos)) != std::string::npos) {
    string.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}

class GraphPrinter : public IRPrinter {
  std::stringstream stream;
  bool colors;

  bool in_colored_block = false;
  bool in_bold_block = false;

 public:
  void write_string(std::string_view s) override {
    if (s == "\n") {
      return;
    }

    std::string string = std::string(s);
    replace_occurrences(string, "&", "&amp;");
    replace_occurrences(string, "<", "&lt;");
    replace_occurrences(string, ">", "&gt;");

    bool restore_color = false;

    // Required for proper SVG generation (Graphviz SVG generator is buggy).
    if (!in_colored_block) {
      begin_color(colors::default_text);
      restore_color = true;
    }

    stream << string;

    if (restore_color) {
      end_color();
    }
  }

  void begin_color(std::string_view color) {
    if (colors) {
      stream << fmt::format("<font color=\"#{}\">", color);
      if (color == colors::keyword || color == colors::type) {
        in_bold_block = true;
        stream << "<b>";
      }

      in_colored_block = true;
    }
  }

  void end_color() {
    if (colors) {
      if (in_bold_block) {
        in_bold_block = false;
        stream << "</b>";
      }

      in_colored_block = false;
      stream << "</font>";
    }
  }

  void begin_keyword() override { begin_color(colors::keyword); }
  void begin_value() override { begin_color(colors::value); }
  void begin_constant() override { begin_color(colors::constant); }
  void begin_type() override { begin_color(colors::type); }
  void begin_block() override { begin_color(colors::block); }

  void end_keyword() override { end_color(); }
  void end_value() override { end_color(); }
  void end_constant() override { end_color(); }
  void end_type() override { end_color(); }
  void end_block() override { end_color(); }

  explicit GraphPrinter(bool colors) : colors(colors) {}

  std::string str() const { return stream.str(); }
};

void Function::generate_dot_graph_source(std::ostream& output,
                                         bool colors,
                                         IRPrintingMethod method) const {
  output << "digraph G {\n";
  output << fmt::format("bgcolor=\"#{}\"\n", colors::background);

  for (const Block& block : *this) {
    std::string block_title;
    if (!block.is_entry_block()) {
      GraphPrinter printer(colors);
      printer.begin_color(colors::block);
      {
        auto l = printer.create_line_printer();
        l.print(&block, IRPrinter::Item::Colon);
      }
      printer.end_color();

      block_title = printer.str();
    } else {
      GraphPrinter printer(colors);
      print_prototype(printer, false);

      block_title = printer.str();
    }

    output << fmt::format(
      "{} [margin=0.15 shape=box fontname=Consolas color=\"#{}\" label=<{}<br/><br/>",
      block.format(), colors::box_border, block_title);

    std::unordered_set<const Value*> inlined;
    if (method == IRPrintingMethod::Compact) {
      inlined = block.get_inlinable_values();
    }

    for (const Instruction& instruction : block) {
      GraphPrinter printer(colors);

      if (method == IRPrintingMethod::Standard) {
        instruction.print(printer);
      } else {
        if (!instruction.print_compact(printer, inlined)) {
          continue;
        }
      }

      output << printer.str() << "<br align=\"left\" />";
    }

    output << ">];\n";
  }

  for (const Block& block : blocks_) {
    const Instruction* last_instruction = block.last_instruction();

    const auto make_edge = [&output](const Block* from, const Block* to,
                                     const std::string_view color) {
      output << fmt::format("{} -> {} [color={}];\n", from->format(), to->format(), color);
    };

    if (const auto branch = cast<Branch>(last_instruction)) {
      make_edge(&block, branch->get_target(), "blue");
    } else if (const auto cond_branch = cast<CondBranch>(last_instruction)) {
      make_edge(&block, cond_branch->get_true_target(), "green");
      make_edge(&block, cond_branch->get_false_target(), "red");
    }
  }

  output << "}\n";
}

void Function::generate_graph(const std::string& graph_path, IRPrintingMethod method) const {
  std::string format;
  {
    const auto last_dot = graph_path.rfind('.');
    verify(last_dot != std::string::npos, "Failed to find output format");
    format = graph_path.substr(last_dot + 1);
  }

  std::stringstream stream;
  generate_dot_graph_source(stream, true, method);

  const std::string command_line = fmt::format(R"(-T{} -o "{}")", format, graph_path);

  verify(run_process("dot", command_line, stream.str()) == 0,
         "Invoking `dot` to generate graph failed");
}

void Function::debug_graph() const {
  const auto graph_path =
    std::filesystem::temp_directory_path() /
    fmt::format("{}_{}_{}_{}.svg", name(), Environment::current_process_id(),
                Environment::current_thread_id(), Environment::monotonic_timestamp(), (void*)this);
  const auto graph_path_string = graph_path.string();

  generate_graph(graph_path_string);
  std::system(graph_path_string.c_str());
}