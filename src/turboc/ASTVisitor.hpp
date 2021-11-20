#pragma once
#include "AST.hpp"

#include <Flugzeug/Core/Error.hpp>

namespace turboc {

namespace detail {

template <bool Constant> class ASTVisitor {
protected:
  template <typename T> using Argument = std::conditional_t<Constant, const T, T>*;

public:
  // Copy to implementation:
  /*
  void visit_assign_stmt(Argument<AssignStmt> assign_stmt) {}
  void visit_binary_assign_stmt(Argument<BinaryAssignStmt> binary_assign_stmt) {}
  void visit_declare_stmt(Argument<DeclareStmt> declare_stmt) {}
  void visit_while_stmt(Argument<WhileStmt> while_stmt) {}
  void visit_if_stmt(Argument<IfStmt> if_stmt) {}
  void visit_for_stmt(Argument<ForStmt> for_stmt) {}
  void visit_return_stmt(Argument<ReturnStmt> return_stmt) {}
  void visit_break_stmt(Argument<BreakStmt> break_stmt) {}
  void visit_continue_stmt(Argument<ContinueStmt> continue_stmt) {}
  void visit_body_stmt(Argument<BodyStmt> body_stmt) {}
  void visit_variable_expr(Argument<VariableExpr> variable_expr) {}
  void visit_unary_expr(Argument<UnaryExpr> unary_expr) {}
  void visit_binary_expr(Argument<BinaryExpr> binary_expr) {}
  void visit_number_expr(Argument<NumberExpr> number_expr) {}
  void visit_array_expr(Argument<ArrayExpr> array_expr) {}
  void visit_call_expr(Argument<CallExpr> call_expr) {}
  void visit_cast_expr(Argument<CastExpr> cast_expr) {}
   */
};

} // namespace detail

using ASTVisitor = detail::ASTVisitor<false>;
using ConstASTVisitor = detail::ASTVisitor<true>;

namespace visitor {
template <typename TStmt, typename TVisitor>
inline auto visit_statement(TStmt* stmt, TVisitor& visitor) {
  using VisitorType = std::remove_cvref_t<TVisitor>;
  static_assert(std::is_base_of_v<ASTVisitor, VisitorType> ||
                  std::is_base_of_v<ConstASTVisitor, VisitorType>,
                "Cannot visit using visitor that is not derived from ASTVisitor");

  switch (stmt->get_kind()) {
  case Stmt::Kind::Assign:
    return visitor.visit_assign_stmt(cast<AssignStmt>(stmt));
  case Stmt::Kind::BinaryAssign:
    return visitor.visit_binary_assign_stmt(cast<BinaryAssignStmt>(stmt));
  case Stmt::Kind::Declare:
    return visitor.visit_declare_stmt(cast<DeclareStmt>(stmt));
  case Stmt::Kind::While:
    return visitor.visit_while_stmt(cast<WhileStmt>(stmt));
  case Stmt::Kind::If:
    return visitor.visit_if_stmt(cast<IfStmt>(stmt));
  case Stmt::Kind::For:
    return visitor.visit_for_stmt(cast<ForStmt>(stmt));
  case Stmt::Kind::Return:
    return visitor.visit_return_stmt(cast<ReturnStmt>(stmt));
  case Stmt::Kind::Break:
    return visitor.visit_break_stmt(cast<BreakStmt>(stmt));
  case Stmt::Kind::Continue:
    return visitor.visit_continue_stmt(cast<ContinueStmt>(stmt));
  case Stmt::Kind::Body:
    return visitor.visit_body_stmt(cast<BodyStmt>(stmt));
  case Stmt::Kind::Variable:
    return visitor.visit_variable_expr(cast<VariableExpr>(stmt));
  case Stmt::Kind::Unary:
    return visitor.visit_unary_expr(cast<UnaryExpr>(stmt));
  case Stmt::Kind::Binary:
    return visitor.visit_binary_expr(cast<BinaryExpr>(stmt));
  case Stmt::Kind::Number:
    return visitor.visit_number_expr(cast<NumberExpr>(stmt));
  case Stmt::Kind::Array:
    return visitor.visit_array_expr(cast<ArrayExpr>(stmt));
  case Stmt::Kind::Call:
    return visitor.visit_call_expr(cast<CallExpr>(stmt));
  case Stmt::Kind::Cast:
    return visitor.visit_cast_expr(cast<CastExpr>(stmt));
  default:
    unreachable();
  }
}
} // namespace visitor

} // namespace turboc