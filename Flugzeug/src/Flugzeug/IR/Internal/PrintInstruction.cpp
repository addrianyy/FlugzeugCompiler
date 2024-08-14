#include "PrintInstruction.hpp"
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/Instructions.hpp>

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
  p.print(to_string(op()), type(), value());
}

void BinaryInstr::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print(to_string(op()), type(), lhs(), SpecialItem::Comma, rhs());
}

void IntCompare::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("cmp", to_string(predicate()), lhs()->type(), lhs(), SpecialItem::Comma, rhs());
}

void Load::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("load", type(), SpecialItem::Comma, address()->type(), address());
}

void Store::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("store", address()->type(), address(), SpecialItem::Comma, value()->type(), value());
}

void Call::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("call", type(), IRPrinter::NonKeywordWord{callee()->name()}, SpecialItem::ParenOpen);

  for (size_t i = 0; i < argument_count(); ++i) {
    const auto arg = argument(i);
    p.print(arg->type(), arg, SpecialItem::Comma);
  }

  p.print(SpecialItem::ParenClose);
}

void Branch::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("branch", target());
}

void CondBranch::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("bcond", condition()->type(), condition(), SpecialItem::Comma, true_target(),
          SpecialItem::Comma, false_target());
}

void StackAlloc::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("stackalloc", allocated_type());

  if (size_ != 1) {
    p.print(SpecialItem::Comma, size_);
  }
}

void Ret::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("ret");

  if (returns_void()) {
    p.print(context()->void_ty());
  } else {
    p.print(return_value()->type(), return_value());
  }
}

void Offset::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("offset", base()->type(), base(), SpecialItem::Comma, index()->type(), index());
}

void Cast::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print(to_string(cast_kind()), casted_value()->type(), casted_value(), "to", type());
}

void Select::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("select", condition()->type(), condition(), SpecialItem::Comma, true_value()->type(),
          true_value(), SpecialItem::Comma, false_value());
}

void Phi::print_instruction_internal(IRPrinter::LinePrinter& p) const {
  p.print("phi", type(), SpecialItem::BracketOpen);

  for (size_t i = 0; i < incoming_count(); ++i) {
    const auto incoming = get_incoming(i);
    p.print(incoming.block, SpecialItem::Colon, incoming.value, SpecialItem::Comma);
  }

  p.print(SpecialItem::BracketClose);
}

void UnaryInstr::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  const auto symbol = to_symbol(op());

  p.print(IRPrinter::UnaryMathSymbol{symbol});
  print_value_compact(value(), p, inlined_values);
}

void BinaryInstr::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  const auto symbol = to_symbol(op());

  print_value_compact(lhs(), p, inlined_values);
  p.print(IRPrinter::BinaryMathSymbol{symbol});
  print_value_compact(rhs(), p, inlined_values);
}

void IntCompare::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  const auto symbol = to_symbol(predicate());

  print_value_compact(lhs(), p, inlined_values);
  p.print(IRPrinter::BinaryMathSymbol{symbol});
  print_value_compact(rhs(), p, inlined_values);
}

void Load::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print("load", type(), IRPrinter::Item::Comma, address()->type());
  print_value_compact(address(), p, inlined_values);
}

void Store::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print("store", address()->type());
  print_value_compact(address(), p, inlined_values);
  p.print(IRPrinter::Item::Comma, value()->type());
  print_value_compact(value(), p, inlined_values);
}

void Call::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print("call", type(), IRPrinter::NonKeywordWord{callee()->name()}, SpecialItem::ParenOpen);

  for (size_t i = 0; i < argument_count(); ++i) {
    print_value_compact(argument(i), p, inlined_values, false);
    p.print(SpecialItem::Comma);
  }

  p.print(SpecialItem::ParenClose);
}

void Branch::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print("branch", target());
}

void CondBranch::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print("bcond");
  print_value_compact(condition(), p, inlined_values, false);
  p.print(SpecialItem::Comma, true_target(), SpecialItem::Comma, false_target());
}

void StackAlloc::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print("stackalloc", allocated_type());

  if (size_ != 1) {
    p.print(SpecialItem::Comma, size_);
  }
}

void Ret::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print("ret");

  if (returns_void()) {
    p.print(context()->void_ty());
  } else {
    p.print(return_value()->type());
    print_value_compact(return_value(), p, inlined_values, false);
  }
}

void Offset::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  print_value_compact(base(), p, inlined_values);
  p.print("offset by");
  print_value_compact(index(), p, inlined_values);
}

void Cast::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print(to_string(cast_kind()), type());
  print_value_compact(casted_value(), p, inlined_values);
}

void Select::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  print_value_compact(condition(), p, inlined_values);
  p.print(IRPrinter::BinaryMathSymbol{"?"});
  print_value_compact(true_value(), p, inlined_values);
  p.print(IRPrinter::BinaryMathSymbol{":"});
  print_value_compact(false_value(), p, inlined_values);
}

void Phi::print_instruction_compact_internal(
  IRPrinter::LinePrinter& p,
  const std::unordered_set<const Value*>& inlined_values) const {
  p.print("phi", type(), SpecialItem::BracketOpen);

  for (auto incoming : *this) {
    p.print(incoming.block, SpecialItem::Colon);
    print_value_compact(incoming.value, p, inlined_values, false);
    p.print(SpecialItem::Comma);
  }

  p.print(SpecialItem::BracketClose);
}