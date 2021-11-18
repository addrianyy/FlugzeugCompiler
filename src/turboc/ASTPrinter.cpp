#include "ASTPrinter.hpp"

#include <Flugzeug/Core/StringifyEnum.hpp>

#include <iostream>

using namespace turboc;

// clang-format off
BEGIN_ENUM_STRINGIFY(stringify_unary_op, UnaryOp)
  ENUM_CASE(Neg)
  ENUM_CASE(Not)
  ENUM_CASE(Ref)
  ENUM_CASE(Deref)
END_ENUM_STRINGIFY()

BEGIN_ENUM_STRINGIFY(stringify_binary_op, BinaryOp)
  ENUM_CASE(Add)
  ENUM_CASE(Sub)
  ENUM_CASE(Mul)
  ENUM_CASE(Mod)
  ENUM_CASE(Div)
  ENUM_CASE(Shr)
  ENUM_CASE(Shl)
  ENUM_CASE(And)
  ENUM_CASE(Or)
  ENUM_CASE(Xor)
  ENUM_CASE(Equal)
  ENUM_CASE(NotEqual)
  ENUM_CASE(Gt)
  ENUM_CASE(Lt)
  ENUM_CASE(Gte)
  ENUM_CASE(Lte)
END_ENUM_STRINGIFY()
// clang-format on

void ASTPrinter::increase_indentation() { indentation_str += "  "; }
void ASTPrinter::decrease_indentation() { indentation_str = indentation_str.substr(2); }

void ASTPrinter::print(std::string_view str) { std::cout << str; }

void ASTPrinter::key_internal(std::string_view name) {
  print(indentation_str);
  print(name);
  print(": ");
}

void ASTPrinter::begin_structure(std::string_view name) {
  print(name);
  print(" {\n");
  increase_indentation();
}

void ASTPrinter::end_structure() {
  decrease_indentation();
  print(indentation_str + "}");
}

void ASTPrinter::standalone_statement(const Stmt* stmt) {
  print(indentation_str);
  stmt->print(*this);
  print("\n");
}

void ASTPrinter::simple_structure(std::string_view str) { print(str); }

void ASTPrinter::value(const Type& type) {
  const auto s = type.format();
  print(s);
}

void ASTPrinter::value(UnaryOp op) { print(stringify_unary_op(op)); }
void ASTPrinter::value(BinaryOp op) { print(stringify_binary_op(op)); }
void ASTPrinter::value(std::string_view str) { print(str); }
void ASTPrinter::value(const Stmt* stmt) {
  if (stmt) {
    stmt->print(*this);
  } else {
    value("none");
  }
}

void ASTPrinter::value(uint64_t num) {
  const auto s = std::to_string(num);
  value(s);
}