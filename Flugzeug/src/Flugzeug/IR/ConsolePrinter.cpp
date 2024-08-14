#include "ConsolePrinter.hpp"

#include <Flugzeug/Core/ConsoleColors.hpp>

#include <iostream>

using namespace flugzeug;

void ConsolePrinter::reset() {
  if (variant == Variant::Colorful) {
    ConsoleColors::reset_color(output_stream);
  }
}

void ConsolePrinter::set_color(int color) {
  if (variant == Variant::Colorful) {
    ConsoleColors::set_color(output_stream, color);
  }
}

void ConsolePrinter::begin_keyword() {
  set_color(32);
}
void ConsolePrinter::begin_value() {
  set_color(33);
}
void ConsolePrinter::begin_constant() {}
void ConsolePrinter::begin_type() {
  set_color(34);
}
void ConsolePrinter::begin_block() {
  set_color(37);
}

void ConsolePrinter::write_string(std::string_view string) {
  output_stream << string;
}

ConsolePrinter::ConsolePrinter(ConsolePrinter::Variant variant)
    : ConsolePrinter(variant, std::cout) {}

ConsolePrinter::ConsolePrinter(ConsolePrinter::Variant variant, std::ostream& output_stream)
    : variant(variant), output_stream(output_stream) {
  ConsoleColors::ensure_initialized();

  if (variant == Variant::ColorfulIfSupported) {
    if (ConsoleColors::supported()) {
      this->variant = Variant::Colorful;
    } else {
      this->variant = Variant::Simple;
    }
  }

  reset();
}

ConsolePrinter::~ConsolePrinter() {
  output_stream.flush();
}