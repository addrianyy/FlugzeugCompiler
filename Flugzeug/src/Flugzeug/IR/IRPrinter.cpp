#include "IRPrinter.hpp"
#include "Block.hpp"

using namespace flugzeug;

void IRPrinter::LinePrinter::begin_generic_item() {
  if (comma_pending) {
    ir_printer->write_string(", ");
  } else if (space_pending) {
    ir_printer->write_string(" ");
  }

  comma_pending = false;
  space_pending = false;
}

void IRPrinter::LinePrinter::end_generic_item() {
  space_pending = true;
  comma_pending = false;
}

void IRPrinter::LinePrinter::item(const Type* type) {
  begin_generic_item();

  ir_printer->begin_type();

  const auto s = type->format();
  ir_printer->write_string(s);

  ir_printer->end_type();

  end_generic_item();
}

void IRPrinter::LinePrinter::item(const Value* value) {
  begin_generic_item();

  const bool is_constant = cast<Constant>(value) || cast<Undef>(value);

  if (is_constant) {
    ir_printer->begin_constant();
  } else {
    ir_printer->begin_value();
  }

  const auto s = value->format();
  ir_printer->write_string(s);

  if (is_constant) {
    ir_printer->end_constant();
  } else {
    ir_printer->end_value();
  }

  end_generic_item();
}

void IRPrinter::LinePrinter::item(const Block* block) {
  begin_generic_item();

  ir_printer->begin_block();

  const auto s = block->format();
  ir_printer->write_string(s);

  ir_printer->end_block();

  end_generic_item();
}

void IRPrinter::LinePrinter::item(std::string_view keyword) {
  begin_generic_item();

  ir_printer->begin_keyword();

  ir_printer->write_string(keyword);

  ir_printer->end_keyword();

  end_generic_item();
}

void IRPrinter::LinePrinter::item(size_t num) {
  begin_generic_item();

  ir_printer->begin_constant();

  const auto s = std::to_string(num);
  ir_printer->write_string(s);

  ir_printer->end_constant();

  end_generic_item();
}

void IRPrinter::LinePrinter::item(IRPrinter::LinePrinter::NonKeywordWord word) {
  begin_generic_item();

  ir_printer->write_string(word.text);

  end_generic_item();
}

void IRPrinter::LinePrinter::item(IRPrinter::LinePrinter::SpecialItem special) {
  comma_pending = false;

  switch (special) {
  case SpecialItem::Comma:
    comma_pending = true;
    space_pending = true;
    break;

  case SpecialItem::Colon:
    ir_printer->write_string(":");
    space_pending = true;
    break;

  case SpecialItem::Equals:
    if (space_pending) {
      ir_printer->write_string(" ");
    }
    ir_printer->write_string("=");
    space_pending = true;
    break;

  case SpecialItem::ParenOpen:
    ir_printer->write_string("(");
    space_pending = false;
    break;

  case SpecialItem::ParenClose:
    ir_printer->write_string(")");
    space_pending = false;
    break;

  case SpecialItem::BracketOpen:
    if (space_pending) {
      ir_printer->write_string(" ");
    }
    ir_printer->write_string("[");
    space_pending = false;
    break;

  case SpecialItem::BracketClose:
    ir_printer->write_string("]");
    space_pending = false;
    break;
  }
}