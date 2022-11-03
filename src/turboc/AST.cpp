#include "AST.hpp"
#include "ASTPrinter.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace turboc;

int32_t turboc::get_binary_op_precedence(BinaryOp op) {
  switch (op) {
    case BinaryOp::Mul:
    case BinaryOp::Mod:
    case BinaryOp::Div:
      return 60;
    case BinaryOp::Add:
    case BinaryOp::Sub:
      return 50;
    case BinaryOp::Shl:
    case BinaryOp::Shr:
      return 40;
    case BinaryOp::Gt:
    case BinaryOp::Lt:
    case BinaryOp::Gte:
    case BinaryOp::Lte:
      return 35;
    case BinaryOp::Equal:
    case BinaryOp::NotEqual:
      return 33;
    case BinaryOp::And:
      return 30;
    case BinaryOp::Xor:
      return 20;
    case BinaryOp::Or:
      return 10;
    default:
      unreachable();
  }
}
void AssignStmt::print(ASTPrinter& printer) const {
  printer.begin_structure("AssignStmt");

  printer.key_value("variable", variable.get());
  printer.key_value("value", value.get());

  printer.end_structure();
}

void BinaryAssignStmt::print(ASTPrinter& printer) const {
  printer.begin_structure("BinaryAssignStmt");

  printer.key_value("variable", variable.get());
  printer.key_value("binary op", op);
  printer.key_value("value", value.get());

  printer.end_structure();
}

void DeclareStmt::print(ASTPrinter& printer) const {
  printer.begin_structure("DeclareStmt");

  printer.key_value("type", type);
  printer.key_value("declaration type", declaration_type);
  printer.key_value("name", name);
  printer.key_value("value", value.get());
  printer.key_value("array size", array_size.get());

  printer.end_structure();
}

void WhileStmt::print(ASTPrinter& printer) const {
  printer.begin_structure("WhileStmt");

  printer.key_value("condition", condition.get());
  printer.key_value("body", body.get());

  printer.end_structure();
}

void IfStmt::print(ASTPrinter& printer) const {
  printer.begin_structure("IfStmt");

  for (size_t i = 0; i < arms.size(); ++i) {
    const std::string arm_name = "arm " + std::to_string(i);
    printer.key(arm_name, [&]() {
      printer.begin_structure("IfArm");
      printer.key_value("condition", arms[i].first.get());
      printer.key_value("body", arms[i].second.get());
      printer.end_structure();
    });
  }

  printer.key_value("default body", default_body.get());

  printer.end_structure();
}

void ForStmt::print(ASTPrinter& printer) const {
  printer.begin_structure("ForStmt");

  printer.key_value("init", init.get());
  printer.key_value("condition", condition.get());
  printer.key_value("step", step.get());
  printer.key_value("body", body.get());

  printer.end_structure();
}

void ReturnStmt::print(ASTPrinter& printer) const {
  printer.begin_structure("ReturnStmt");

  printer.key_value("return value", return_value.get());

  printer.end_structure();
}

void BreakStmt::print(ASTPrinter& printer) const {
  printer.simple_structure("BreakStmt");
}

void ContinueStmt::print(ASTPrinter& printer) const {
  printer.simple_structure("ContinueStmt");
}

void BodyStmt::print(ASTPrinter& printer) const {
  printer.begin_structure("BodyStmt");

  for (const auto& stmt : statements) {
    printer.standalone_statement(stmt.get());
  }

  printer.end_structure();
}

void VariableExpr::print(ASTPrinter& printer) const {
  const std::string s = "VariableExpr { " + name + " }";
  printer.simple_structure(s);
}

void UnaryExpr::print(ASTPrinter& printer) const {
  printer.begin_structure("UnaryExpr");

  printer.key_value("unary op", op);
  printer.key_value("value", value.get());

  printer.end_structure();
}

void BinaryExpr::print(ASTPrinter& printer) const {
  printer.begin_structure("BinaryExpr");

  printer.key_value("lhs", left.get());
  printer.key_value("binary op", op);
  printer.key_value("rhs", right.get());

  printer.end_structure();
}

void NumberExpr::print(ASTPrinter& printer) const {
  const std::string s = "NumberExpr { " + type.format() + " " + std::to_string(value) + " }";
  printer.simple_structure(s);
}

void ArrayExpr::print(ASTPrinter& printer) const {
  printer.begin_structure("ArrayExpr");

  printer.key_value("array", array.get());
  printer.key_value("index", index.get());

  printer.end_structure();
}

void CallExpr::print(ASTPrinter& printer) const {
  printer.begin_structure("CallExpr");

  printer.key_value("function", function_name);

  for (size_t i = 0; i < arguments.size(); ++i) {
    const std::string key = "argument " + std::to_string(i);
    printer.key_value(key, arguments[i].get());
  }

  printer.end_structure();
}

void CastExpr::print(ASTPrinter& printer) const {
  printer.begin_structure("CastExpr");

  printer.key_value("value", value.get());
  printer.key_value("type", type);

  printer.end_structure();
}