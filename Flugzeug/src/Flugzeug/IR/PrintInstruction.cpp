#include "Function.hpp"
#include "Instructions.hpp"

using namespace flugzeug;

using SpecialItem = IRPrinter::LinePrinter::SpecialItem;

static std::string_view to_string(UnaryOp op) {
  switch (op) {
  case UnaryOp::Neg:
    return "neg";
  case UnaryOp::Not:
    return "not";
  default:
    unreachable();
  }
}

static std::string_view to_string(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add:
    return "add";
  case BinaryOp::Sub:
    return "sub";
  case BinaryOp::Mul:
    return "mul";
  case BinaryOp::ModS:
    return "smod";
  case BinaryOp::DivS:
    return "sdiv";
  case BinaryOp::ModU:
    return "umod";
  case BinaryOp::DivU:
    return "udiv";
  case BinaryOp::Shr:
    return "shr";
  case BinaryOp::Shl:
    return "shl";
  case BinaryOp::Sar:
    return "sar";
  case BinaryOp::And:
    return "and";
  case BinaryOp::Or:
    return "or";
  case BinaryOp::Xor:
    return "xor";
  default:
    unreachable();
  }
}

static std::string_view to_string(IntPredicate pred) {
  switch (pred) {
  case IntPredicate::Equal:
    return "eq";
  case IntPredicate::NotEqual:
    return "ne";
  case IntPredicate::GtU:
    return "ugt";
  case IntPredicate::GteU:
    return "ugte";
  case IntPredicate::GtS:
    return "sgt";
  case IntPredicate::GteS:
    return "sgte";
  case IntPredicate::LtU:
    return "ult";
  case IntPredicate::LteU:
    return "ulte";
  case IntPredicate::LtS:
    return "slt";
  case IntPredicate::LteS:
    return "slte";
  default:
    unreachable();
  }
}

static std::string_view to_string(CastKind cast) {
  switch (cast) {
  case CastKind::ZeroExtend:
    return "zext";
  case CastKind::SignExtend:
    return "sext";
  case CastKind::Truncate:
    return "trunc";
  case CastKind::Bitcast:
    return "bitcast";
  default:
    unreachable();
  }
}

static std::string_view to_symbol(UnaryOp op) {
  switch (op) {
  case UnaryOp::Neg:
    return "-";
  case UnaryOp::Not:
    return "~";
  default:
    unreachable();
  }
}

static std::string_view to_symbol(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add:
    return "+";
  case BinaryOp::Sub:
    return "-";
  case BinaryOp::Mul:
    return "*";
  case BinaryOp::ModU:
    return "%u";
  case BinaryOp::DivU:
    return "/u";
  case BinaryOp::ModS:
    return "%s";
  case BinaryOp::DivS:
    return "/s";
  case BinaryOp::Shr:
    return ">>";
  case BinaryOp::Shl:
    return "<<";
  case BinaryOp::Sar:
    return ">>>";
  case BinaryOp::And:
    return "&";
  case BinaryOp::Or:
    return "|";
  case BinaryOp::Xor:
    return "^";
  default:
    unreachable();
  }
}

static std::string_view to_symbol(IntPredicate predicate) {
  switch (predicate) {
  case IntPredicate::Equal:
    return "==";
  case IntPredicate::NotEqual:
    return "!=";
  case IntPredicate::GtU:
    return ">u";
  case IntPredicate::GteU:
    return ">=u";
  case IntPredicate::GtS:
    return ">s";
  case IntPredicate::GteS:
    return ">=s";
  case IntPredicate::LtU:
    return "<u";
  case IntPredicate::LteU:
    return "<=u";
  case IntPredicate::LtS:
    return "<s";
  case IntPredicate::LteS:
    return "<=s";
  default:
    unreachable();
  }
}

void UnaryInstr::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print(to_string(get_op()), get_type(), get_val());
}

void BinaryInstr::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print(to_string(get_op()), get_type(), get_lhs(), SpecialItem::Comma, get_rhs());
}

void IntCompare::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("cmp", to_string(get_pred()), get_lhs()->get_type(), get_lhs(), SpecialItem::Comma,
          get_rhs());
}

void Load::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("load", get_type(), SpecialItem::Comma, get_ptr()->get_type(), get_ptr());
}

void Store::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("store", get_ptr()->get_type(), get_ptr(), SpecialItem::Comma, get_val()->get_type(),
          get_val());
}

void Call::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("call", get_type(), IRPrinter::NonKeywordWord{get_callee()->get_name()},
          SpecialItem::ParenOpen);

  for (size_t i = 0; i < get_arg_count(); ++i) {
    const auto arg = get_arg(i);
    p.print(arg->get_type(), arg, SpecialItem::Comma);
  }

  p.print(SpecialItem::ParenClose);
}

void Branch::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("branch", get_target());
}

void CondBranch::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("bcond", get_cond()->get_type(), get_cond(), SpecialItem::Comma, get_true_target(),
          SpecialItem::Comma, get_false_target());
}

void StackAlloc::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("stackalloc", get_allocated_type());

  if (size != 1) {
    p.print(SpecialItem::Comma, size);
  }
}

void Ret::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("ret");

  if (is_ret_void()) {
    p.print(get_context()->get_void_ty());
  } else {
    p.print(get_val()->get_type(), get_val());
  }
}

void Offset::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("offset", get_base()->get_type(), get_base(), SpecialItem::Comma, get_index()->get_type(),
          get_index());
}

void Cast::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print(to_string(get_cast_kind()), get_val()->get_type(), get_val(), "to", get_type());
}

void Select::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("select", get_cond()->get_type(), get_cond(), SpecialItem::Comma,
          get_true_val()->get_type(), get_true_val(), SpecialItem::Comma, get_false_val());
}

void Phi::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("phi", get_type(), SpecialItem::BracketOpen);

  for (size_t i = 0; i < get_incoming_count(); ++i) {
    const auto incoming = get_incoming(i);
    p.print(incoming.block, SpecialItem::Colon, incoming.value, SpecialItem::Comma);
  }

  p.print(SpecialItem::BracketClose);
}

void UnaryInstr::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  const auto symbol = to_symbol(get_op());

  p.print(IRPrinter::UnaryMathSymbol{symbol});
  print_possibly_inlined_value(get_val(), p, inlined_values);
}

void BinaryInstr::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  const auto symbol = to_symbol(get_op());

  print_possibly_inlined_value(get_lhs(), p, inlined_values);
  p.print(IRPrinter::BinaryMathSymbol{symbol});
  print_possibly_inlined_value(get_rhs(), p, inlined_values);
}

void IntCompare::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  const auto symbol = to_symbol(get_pred());

  print_possibly_inlined_value(get_lhs(), p, inlined_values);
  p.print(IRPrinter::BinaryMathSymbol{symbol});
  print_possibly_inlined_value(get_rhs(), p, inlined_values);
}

void Load::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print("load", get_type(), IRPrinter::Item::Comma, get_ptr()->get_type());
  print_possibly_inlined_value(get_ptr(), p, inlined_values);
}

void Store::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print("store", get_ptr()->get_type());
  print_possibly_inlined_value(get_ptr(), p, inlined_values);
  p.print(IRPrinter::Item::Comma, get_val()->get_type());
  print_possibly_inlined_value(get_val(), p, inlined_values);
}

void Call::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print("call", get_type(), IRPrinter::NonKeywordWord{get_callee()->get_name()},
          SpecialItem::ParenOpen);

  for (size_t i = 0; i < get_arg_count(); ++i) {
    print_possibly_inlined_value(get_arg(i), p, inlined_values, false);
    p.print(SpecialItem::Comma);
  }

  p.print(SpecialItem::ParenClose);
}

void Branch::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print("branch", get_target());
}

void CondBranch::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print("bcond");
  print_possibly_inlined_value(get_cond(), p, inlined_values, false);
  p.print(SpecialItem::Comma, get_true_target(), SpecialItem::Comma, get_false_target());
}

void StackAlloc::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print("stackalloc", get_allocated_type());

  if (size != 1) {
    p.print(SpecialItem::Comma, size);
  }
}

void Ret::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print("ret");

  if (is_ret_void()) {
    p.print(get_context()->get_void_ty());
  } else {
    p.print(get_val()->get_type());
    print_possibly_inlined_value(get_val(), p, inlined_values, false);
  }
}

void Offset::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  print_possibly_inlined_value(get_base(), p, inlined_values);
  p.print("offset by");
  print_possibly_inlined_value(get_index(), p, inlined_values);
}

void Cast::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print(to_string(get_cast_kind()), get_type());
  print_possibly_inlined_value(get_val(), p, inlined_values);
}

void Select::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  print_possibly_inlined_value(get_cond(), p, inlined_values);
  p.print(IRPrinter::BinaryMathSymbol{"?"});
  print_possibly_inlined_value(get_true_val(), p, inlined_values);
  p.print(IRPrinter::BinaryMathSymbol{":"});
  print_possibly_inlined_value(get_false_val(), p, inlined_values);
}

void Phi::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p, const std::unordered_set<const Value*>& inlined_values) const {
  p.print("phi", get_type(), SpecialItem::BracketOpen);

  for (auto incoming : *this) {
    p.print(incoming.block, SpecialItem::Colon);
    print_possibly_inlined_value(incoming.value, p, inlined_values);
    p.print(SpecialItem::Comma);
  }

  p.print(SpecialItem::BracketClose);
}