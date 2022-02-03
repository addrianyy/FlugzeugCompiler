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