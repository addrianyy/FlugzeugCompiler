#pragma once
#include "Lexer.hpp"
#include "Type.hpp"

#include <Flugzeug/Core/Casting.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace turboc {

class ASTPrinter;
class Expr;
class BodyStmt;

enum class UnaryOp {
  Neg,
  Not,
  Ref,
  Deref,
};

enum class BinaryOp {
  Add,
  Sub,
  Mul,
  Mod,
  Div,
  Shr,
  Shl,
  And,
  Or,
  Xor,
  Equal,
  NotEqual,
  Gt,
  Lt,
  Gte,
  Lte,
};

int32_t get_binary_op_precedence(BinaryOp op);

class Stmt {
 public:
  // clang-format off
  enum class Kind {
    Assign,
    BinaryAssign,
    Declare,
    While,
    If,
    For,
    Return,
    Break,
    Continue,
    Body,
    ExprBegin,
      Variable,
      Unary,
      Binary,
      Number,
      Array,
      Call,
      Cast,
    ExprEnd,
  };
  // clang-format on

 private:
  const Kind kind;

 protected:
  explicit Stmt(Kind kind) : kind(kind) {}

 public:
  virtual ~Stmt() = default;

  Kind get_kind() const { return kind; }

  virtual void print(ASTPrinter& printer) const = 0;
};

class AssignStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Assign);

  std::unique_ptr<Expr> variable;
  std::unique_ptr<Expr> value;

 public:
  AssignStmt(std::unique_ptr<Expr> variable, std::unique_ptr<Expr> value)
      : Stmt(Stmt::Kind::Assign), variable(std::move(variable)), value(std::move(value)) {}

  const std::unique_ptr<Expr>& get_variable() const { return variable; }
  const std::unique_ptr<Expr>& get_value() const { return value; }

  void print(ASTPrinter& printer) const override;
};

class BinaryAssignStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::BinaryAssign);

  std::unique_ptr<Expr> variable;
  BinaryOp op;
  std::unique_ptr<Expr> value;

 public:
  BinaryAssignStmt(std::unique_ptr<Expr> variable, BinaryOp op, std::unique_ptr<Expr> value)
      : Stmt(Stmt::Kind::BinaryAssign),
        variable(std::move(variable)),
        op(op),
        value(std::move(value)) {}

  const std::unique_ptr<Expr>& get_variable() const { return variable; }
  BinaryOp get_op() const { return op; }
  const std::unique_ptr<Expr>& get_value() const { return value; }

  void print(ASTPrinter& printer) const override;
};

class DeclareStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Declare)

  Type type;
  Type declaration_type;

  std::string name;

  std::unique_ptr<Expr> value;
  std::unique_ptr<Expr> array_size;

 public:
  DeclareStmt(Type type,
              Type declaration_type,
              std::string name,
              std::unique_ptr<Expr> value,
              std::unique_ptr<Expr> array_size)
      : Stmt(Stmt::Kind::Declare),
        type(type),
        declaration_type(declaration_type),
        name(std::move(name)),
        value(std::move(value)),
        array_size(std::move(array_size)) {}

  Type get_type() const { return type; }
  Type get_declaration_type() const { return declaration_type; }
  const std::string& get_name() const { return name; }
  const std::unique_ptr<Expr>& get_value() const { return value; }
  const std::unique_ptr<Expr>& get_array_size() const { return array_size; }

  void print(ASTPrinter& printer) const override;
};

class WhileStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::While)

  std::unique_ptr<Expr> condition;
  std::unique_ptr<BodyStmt> body;

 public:
  WhileStmt(std::unique_ptr<Expr> condition, std::unique_ptr<BodyStmt> body)
      : Stmt(Stmt::Kind::While), condition(std::move(condition)), body(std::move(body)) {}

  const std::unique_ptr<Expr>& get_condition() const { return condition; }
  const std::unique_ptr<BodyStmt>& get_body() const { return body; }

  void print(ASTPrinter& printer) const override;
};

class IfStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::If)

 public:
  using Arm = std::pair<std::unique_ptr<Expr>, std::unique_ptr<BodyStmt>>;

 private:
  std::vector<Arm> arms;
  std::unique_ptr<BodyStmt> default_body;

 public:
  IfStmt(std::vector<Arm> arms, std::unique_ptr<BodyStmt> default_body)
      : Stmt(Stmt::Kind::If), arms(std::move(arms)), default_body(std::move(default_body)) {}

  const std::vector<Arm>& get_arms() const { return arms; }
  const std::unique_ptr<BodyStmt>& get_default_body() const { return default_body; }

  void print(ASTPrinter& printer) const override;
};

class ForStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::For)

  std::unique_ptr<Stmt> init;
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Stmt> step;
  std::unique_ptr<BodyStmt> body;

 public:
  ForStmt(std::unique_ptr<Stmt> init,
          std::unique_ptr<Expr> condition,
          std::unique_ptr<Stmt> step,
          std::unique_ptr<BodyStmt> body)
      : Stmt(Stmt::Kind::For),
        init(std::move(init)),
        condition(std::move(condition)),
        step(std::move(step)),
        body(std::move(body)) {}

  const std::unique_ptr<Stmt>& get_init() const { return init; }
  const std::unique_ptr<Expr>& get_condition() const { return condition; }
  const std::unique_ptr<Stmt>& get_step() const { return step; }
  const std::unique_ptr<BodyStmt>& get_body() const { return body; }

  void print(ASTPrinter& printer) const override;
};

class ReturnStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Return)

  std::unique_ptr<Expr> return_value;

 public:
  explicit ReturnStmt(std::unique_ptr<Expr> return_value)
      : Stmt(Stmt::Kind::Return), return_value(std::move(return_value)) {}

  const std::unique_ptr<Expr>& get_return_value() const { return return_value; }

  void print(ASTPrinter& printer) const override;
};

class BreakStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Break)

 public:
  BreakStmt() : Stmt(Stmt::Kind::Break) {}

  void print(ASTPrinter& printer) const override;
};

class ContinueStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Continue)

 public:
  ContinueStmt() : Stmt(Stmt::Kind::Continue) {}

  void print(ASTPrinter& printer) const override;
};

class BodyStmt : public Stmt {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Body)

  std::vector<std::unique_ptr<Stmt>> statements;

 public:
  explicit BodyStmt(std::vector<std::unique_ptr<Stmt>> statements)
      : Stmt(Stmt::Kind::Body), statements(std::move(statements)) {}

  const std::vector<std::unique_ptr<Stmt>>& get_statements() const { return statements; }

  void print(ASTPrinter& printer) const override;
};

class Expr : public Stmt {
  DEFINE_INSTANCEOF_RANGE(Stmt, Stmt::Kind::ExprBegin, Stmt::Kind::ExprEnd)

 protected:
  using Stmt::Stmt;
};

class VariableExpr : public Expr {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Variable)

  std::string name;

 public:
  explicit VariableExpr(std::string name) : Expr(Stmt::Kind::Variable), name(std::move(name)) {}

  const std::string& get_name() const { return name; }

  void print(ASTPrinter& printer) const override;
};

class UnaryExpr : public Expr {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Unary)

  UnaryOp op;
  std::unique_ptr<Expr> value;

 public:
  UnaryExpr(UnaryOp op, std::unique_ptr<Expr> value)
      : Expr(Stmt::Kind::Unary), op(op), value(std::move(value)) {}

  UnaryOp get_op() const { return op; }
  const std::unique_ptr<Expr>& get_value() const { return value; }

  void print(ASTPrinter& printer) const override;
};

class BinaryExpr : public Expr {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Binary)

  std::unique_ptr<Expr> left;
  BinaryOp op;
  std::unique_ptr<Expr> right;

 public:
  BinaryExpr(std::unique_ptr<Expr> left, BinaryOp op, std::unique_ptr<Expr> right)
      : Expr(Stmt::Kind::Binary), left(std::move(left)), op(op), right(std::move(right)) {}

  const std::unique_ptr<Expr>& get_left() const { return left; }
  BinaryOp get_op() const { return op; }
  const std::unique_ptr<Expr>& get_right() const { return right; }

  void print(ASTPrinter& printer) const override;
};

class NumberExpr : public Expr {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Number)

  Type type;
  uint64_t value;

 public:
  NumberExpr(Type type, uint64_t value) : Expr(Stmt::Kind::Number), type(type), value(value) {}

  Type get_type() const { return type; }
  uint64_t get_value() const { return value; }

  void print(ASTPrinter& printer) const override;
};

class ArrayExpr : public Expr {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Array)

  std::unique_ptr<Expr> array;
  std::unique_ptr<Expr> index;

 public:
  ArrayExpr(std::unique_ptr<Expr> array, std::unique_ptr<Expr> index)
      : Expr(Stmt::Kind::Array), array(std::move(array)), index(std::move(index)) {}

  const std::unique_ptr<Expr>& get_array() const { return array; }
  const std::unique_ptr<Expr>& get_index() const { return index; }

  void print(ASTPrinter& printer) const override;
};

class CallExpr : public Expr {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Call)

  std::string function_name;
  std::vector<std::unique_ptr<Expr>> arguments;

 public:
  CallExpr(std::string function_name, std::vector<std::unique_ptr<Expr>> arguments)
      : Expr(Stmt::Kind::Call),
        function_name(std::move(function_name)),
        arguments(std::move(arguments)) {}

  const std::string& get_function_name() const { return function_name; }
  const std::vector<std::unique_ptr<Expr>>& get_arguments() const { return arguments; }

  void print(ASTPrinter& printer) const override;
};

class CastExpr : public Expr {
  DEFINE_INSTANCEOF(Stmt, Stmt::Kind::Cast)

  std::unique_ptr<Expr> value;
  Type type;

 public:
  CastExpr(std::unique_ptr<Expr> value, Type type)
      : Expr(Stmt::Kind::Cast), value(std::move(value)), type(type) {}

  const std::unique_ptr<Expr>& get_value() const { return value; }
  Type get_type() const { return type; }

  void print(ASTPrinter& printer) const override;
};

}  // namespace turboc