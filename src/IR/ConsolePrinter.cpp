#include "ConsolePrinter.hpp"

#include <Core/ConsoleColors.hpp>

#include <iostream>

using namespace flugzeug;

void ConsolePrinter::reset() {
  if (variant == Variant::Colorful) {
    std::cout << "\x1b[0m";
  }
}

void ConsolePrinter::set_color(int color) {
  if (variant == Variant::Colorful) {
    std::cout << "\x1b[1;" << color << "m";
  }
}

void ConsolePrinter::begin_keyword() { set_color(32); }
void ConsolePrinter::begin_value() { set_color(33); }
void ConsolePrinter::begin_constant() {}
void ConsolePrinter::begin_type() { set_color(34); }
void ConsolePrinter::begin_block() { set_color(37); }

void ConsolePrinter::write_string(std::string_view string) { std::cout << string; }

ConsolePrinter::ConsolePrinter(ConsolePrinter::Variant variant) : variant(variant) {
  console_colors::ensure_initialized();
  reset();
}