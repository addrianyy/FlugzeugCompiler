#pragma once
#include "ASTVisitor.hpp"
#include "Function.hpp"

#include <Flugzeug/IR/Context.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionInserter.hpp>

namespace turboc {

namespace fz = flugzeug;

class IRGenerator : public ConstASTVisitor {
  using FunctionMap = std::unordered_map<std::string, const Function*>;

  struct Loop {
    fz::Block* continue_label;
    fz::Block* break_label;
  };

  class CodegenValue {
  public:
    enum class Kind {
      Lvalue,
      Rvalue,
    };

  private:
    Kind kind;
    Type type;
    fz::Value* value;

  public:
    CodegenValue(Kind kind, Type type, fz::Value* value) : kind(kind), type(type), value(value) {}

    static inline CodegenValue lvalue(Type type, fz::Value* value) {
      return {Kind::Lvalue, type, value};
    }
    static inline CodegenValue rvalue(Type type, fz::Value* value) {
      return {Kind::Rvalue, type, value};
    }

    fz::Value* get_raw_value() const { return value; }
    bool is_lvalue() const { return kind == Kind::Lvalue; }
    bool is_rvalue() const { return kind == Kind::Rvalue; }
    Type get_type() const { return type; }
  };

  class Variables {
    std::unordered_map<std::string, CodegenValue> variables;
    std::vector<std::vector<std::string>> scopes;

  public:
    Variables() = default;

    void clear();

    void enter_scope();
    void exit_scope();

    void insert(const std::string& name, CodegenValue value);
    CodegenValue get(const std::string& name);
  };

  using VisitResult = std::optional<CodegenValue>;

  template <typename TStmt, typename TVisitor>
  friend auto visitor::visit_statement(TStmt* stmt, TVisitor& visitor);

  fz::Context* context;
  fz::Module* module;

  FunctionMap function_map;

  fz::InstructionInserter inserter;

  const Function* current_function = nullptr;
  fz::Function* current_ir_function = nullptr;
  Variables variables;
  std::vector<Loop> loops;

  const Loop& get_current_loop();

  fz::Type* convert_type(Type type);

  fz::Value* extract_value(const CodegenValue& value);

  fz::Value* int_cast(fz::Value* value, Type from_type, Type to_type);
  fz::Value* implicit_cast(const CodegenValue& value, Type wanted_type);

  std::pair<CodegenValue, CodegenValue> implcit_cast_binary(CodegenValue left, CodegenValue right);
  CodegenValue generate_binary_op(CodegenValue left, BinaryOp op, CodegenValue right);

  fz::Value* generate_condition(const Expr* condition);

  VisitResult visit_assign_stmt(Argument<AssignStmt> assign_stmt);
  VisitResult visit_binary_assign_stmt(Argument<BinaryAssignStmt> binary_assign_stmt);
  VisitResult visit_declare_stmt(Argument<DeclareStmt> declare_stmt);
  VisitResult visit_while_stmt(Argument<WhileStmt> while_stmt);
  VisitResult visit_if_stmt(Argument<IfStmt> if_stmt);
  VisitResult visit_for_stmt(Argument<ForStmt> for_stmt);
  VisitResult visit_return_stmt(Argument<ReturnStmt> return_stmt);
  VisitResult visit_break_stmt(Argument<BreakStmt> break_stmt);
  VisitResult visit_continue_stmt(Argument<ContinueStmt> continue_stmt);
  VisitResult visit_body_stmt(Argument<BodyStmt> body_stmt);
  VisitResult visit_variable_expr(Argument<VariableExpr> variable_expr);
  VisitResult visit_unary_expr(Argument<UnaryExpr> unary_expr);
  VisitResult visit_binary_expr(Argument<BinaryExpr> binary_expr);
  VisitResult visit_number_expr(Argument<NumberExpr> number_expr);
  VisitResult visit_array_expr(Argument<ArrayExpr> array_expr);
  VisitResult visit_call_expr(Argument<CallExpr> call_expr);
  VisitResult visit_cast_expr(Argument<CastExpr> cast_expr);

  CodegenValue generate_nonvoid_expression(const Expr* expr);
  CodegenValue generate_nonvoid_expression(const std::unique_ptr<Expr>& expr) {
    return generate_nonvoid_expression(expr.get());
  }

  std::optional<CodegenValue> generate_expression(const Expr* expr);
  std::optional<CodegenValue> generate_expression(const std::unique_ptr<Expr>& expr) {
    return generate_expression(expr.get());
  }

  void generate_statement(const Stmt* stmt);
  bool generate_body(const BodyStmt* body);
  bool generate_body(const std::unique_ptr<BodyStmt>& body) { return generate_body(body.get()); }

  void generate_local_function(const Function& function, fz::Function* ir_function);
  void create_declarations(const std::vector<Function>& functions);
  void generate_ir_for_functions(const std::vector<Function>& functions);

  explicit IRGenerator(fz::Context* context);

public:
  static fz::Module* generate(fz::Context* context, const std::vector<Function>& functions);
};

} // namespace turboc