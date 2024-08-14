#include "ConsoleIRPrinter.hpp"

#include <Flugzeug/Core/ConsoleColors.hpp>

#include <iostream>

using namespace flugzeug;

void ConsoleIRPrinter::reset() {
  if (variant == Variant::Colorful) {
    ConsoleColors::reset_color(output_stream);
  }
}

void ConsoleIRPrinter::set_color(int color) {
  if (variant == Variant::Colorful) {
    ConsoleColors::set_color(output_stream, color);
  }
}

void ConsoleIRPrinter::begin_keyword() {
  set_color(32);
}
void ConsoleIRPrinter::begin_value() {
  set_color(33);
}
void ConsoleIRPrinter::begin_constant() {}
void ConsoleIRPrinter::begin_type() {
  set_color(34);
}
void ConsoleIRPrinter::begin_block() {
  set_color(37);
}

void ConsoleIRPrinter::write_string(std::string_view string) {
  output_stream << string;
}

ConsoleIRPrinter::ConsoleIRPrinter(ConsoleIRPrinter::Variant variant)
    : ConsoleIRPrinter(variant, std::cout) {}

ConsoleIRPrinter::ConsoleIRPrinter(ConsoleIRPrinter::Variant variant, std::ostream& output_stream)
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

ConsoleIRPrinter::~ConsoleIRPrinter() {
  output_stream.flush();
}