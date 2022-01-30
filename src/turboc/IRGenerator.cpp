#include "IRGenerator.hpp"
#include <Flugzeug/IR/Module.hpp>

using namespace turboc;

static std::optional<uint64_t> evaluate_constant_expression(const Expr* expr) {
  if (const auto number = cast<NumberExpr>(expr)) {
    return number->get_value();
  }

  return std::nullopt;
}

static fz::BinaryOp convert_to_ir_binary_op(BinaryOp op, bool is_signed) {
  // clang-format off
  switch (op) {
    case BinaryOp::Add: return fz::BinaryOp::Add;
    case BinaryOp::Sub: return fz::BinaryOp::Sub;
    case BinaryOp::Mul: return fz::BinaryOp::Mul;
    case BinaryOp::Mod: return is_signed ? fz::BinaryOp::ModS : fz::BinaryOp::ModU;
    case BinaryOp::Div: return is_signed ? fz::BinaryOp::DivS : fz::BinaryOp::DivU;
    case BinaryOp::Shr: return is_signed ? fz::BinaryOp::Sar  : fz::BinaryOp::Shr;
    case BinaryOp::Shl: return fz::BinaryOp::Shl;
    case BinaryOp::And: return fz::BinaryOp::And;
    case BinaryOp::Or:  return fz::BinaryOp::Or;
    case BinaryOp::Xor: return fz::BinaryOp::Xor;
    default:            unreachable();
  }
  // clang-format on
}

static fz::IntPredicate convert_to_ir_int_predicate(BinaryOp op, bool is_signed) {
  // clang-format off
  switch (op) {
    case BinaryOp::Equal:    return fz::IntPredicate::Equal;
    case BinaryOp::NotEqual: return fz::IntPredicate::NotEqual;
    case BinaryOp::Gt:       return is_signed ? fz::IntPredicate::GtS : fz::IntPredicate::GtU;
    case BinaryOp::Lt:       return is_signed ? fz::IntPredicate::LtS : fz::IntPredicate::LtU;
    case BinaryOp::Gte:      return is_signed ? fz::IntPredicate::GteS : fz::IntPredicate::GteU;
    case BinaryOp::Lte:      return is_signed ? fz::IntPredicate::LteS : fz::IntPredicate::LteU;
    default:                 unreachable();
  }
  // clang-format on
}

void IRGenerator::Variables::clear() {
  variables.clear();
  scopes.clear();
}

void IRGenerator::Variables::enter_scope() { scopes.emplace_back(); }

void IRGenerator::Variables::exit_scope() {
  const auto last_scope = std::move(scopes.back());
  scopes.pop_back();

  for (const auto& name : last_scope) {
    verify(variables.erase(name) == 1, "Failed to remove variable after exiting scope");
  }
}

void IRGenerator::Variables::insert(const std::string& name, IRGenerator::CodegenValue value) {
  const auto inserted = variables.insert({name, value}).second;
  verify(inserted, "Variable {} is already defined", name);

  scopes.back().push_back(name);
}

IRGenerator::CodegenValue IRGenerator::Variables::get(const std::string& name) {
  const auto it = variables.find(name);
  verify(it != variables.end(), "Variable {} not found", name);

  return it->second;
}

const IRGenerator::Loop& IRGenerator::get_current_loop() {
  verify(!loops.empty(), "Cannot use break/continue outside of the loop");
  return loops.back();
}

fz::Type* IRGenerator::convert_type(Type type) {
  fz::Type* base = nullptr;

  switch (type.get_kind()) {
  case Type::Kind::U8:
  case Type::Kind::I8:
    base = context->get_i8_ty();
    break;

  case Type::Kind::U16:
  case Type::Kind::I16:
    base = context->get_i16_ty();
    break;

  case Type::Kind::U32:
  case Type::Kind::I32:
    base = context->get_i32_ty();
    break;

  case Type::Kind::I64:
  case Type::Kind::U64:
    base = context->get_i64_ty();
    break;

  case Type::Kind::Void:
    base = context->get_void_ty();
    break;

  default:
    unreachable();
  }

  if (type.get_indirection() > 0) {
    if (base->is_void()) {
      base = context->get_i8_ty();
    }

    return context->get_pointer_type(base, type.get_indirection());
  }

  return base;
}

fz::Value* IRGenerator::extract_value(const CodegenValue& value) {
  if (value.is_lvalue()) {
    return inserter.load(value.get_raw_value());
  }

  return value.get_raw_value();
}

fz::Value* IRGenerator::int_cast(fz::Value* value, Type from_type, Type to_type) {
  verify(!from_type.is_pointer() && !to_type.is_pointer(), "Cannot int cast pointers");

  const auto from_size = from_type.get_byte_size();
  const auto to_size = to_type.get_byte_size();

  const auto to_type_ir = convert_type(to_type);

  if (from_size > to_size) {
    return inserter.trunc(value, to_type_ir);
  } else if (from_size < to_size) {
    const auto cast_kind =
      from_type.is_signed() ? fz::CastKind::SignExtend : fz::CastKind::ZeroExtend;

    return inserter.cast(value, cast_kind, to_type_ir);
  } else {
    return value;
  }
}

fz::Value* IRGenerator::implicit_cast(const CodegenValue& value, Type wanted_type) {
  const auto extracted = extract_value(value);

  if (value.get_type() != wanted_type) {
    return int_cast(extracted, value.get_type(), wanted_type);
  }

  return extracted;
}

std::pair<IRGenerator::CodegenValue, IRGenerator::CodegenValue>
IRGenerator::implcit_cast_binary(CodegenValue left, CodegenValue right) {
  if (left.get_type() == right.get_type()) {
    return {left, right};
  }

  Type left_type = left.get_type();
  Type right_type = right.get_type();

  verify(!left_type.is_pointer() && !right_type.is_pointer(), "Cannot implicit cast pointers");
  verify(!left_type.is_void() && !right_type.is_void(), "Cannot implicit cast void types");

  const auto get_conversion_rank = [](Type type) { return type.get_byte_size(); };

  // https://en.cppreference.com/w/c/language/conversion

  if (left_type.is_signed() == right_type.is_signed()) {
    // If the types have the same signedness (both signed or both unsigned),
    // the operand whose type has the lesser conversion rank is
    // implicitly converted to the other type.

    if (get_conversion_rank(left_type) > get_conversion_rank(right_type)) {
      right_type = left_type;
    } else {
      left_type = right_type;
    }
  } else {
    Type signed_type = left_type;
    Type unsigned_type = right_type;

    if (unsigned_type.is_signed()) {
      std::swap(signed_type, unsigned_type);
    }

    // If the unsigned type has conversion rank greater than or equal
    // to the rank of the signed type, then the operand with the signed
    // type is implicitly converted to the unsigned type.

    // The unsigned type has conversion rank less than the signed type.
    // If the signed type can represent all values of the unsigned type,
    // then the operand with the unsigned type is implicitly converted
    // to the signed type.

    const Type common_type = get_conversion_rank(unsigned_type) >= get_conversion_rank(signed_type)
                               ? unsigned_type
                               : signed_type;

    left_type = common_type;
    right_type = common_type;
  }

  verify(left_type == right_type, "Implicit cast failed (?)");

  const auto cast_single = [&](const CodegenValue& value, Type new_type) {
    if (value.get_type() != new_type) {
      const auto casted = int_cast(extract_value(value), value.get_type(), new_type);
      return CodegenValue::rvalue(new_type, casted);
    }

    return value;
  };

  return {cast_single(left, left_type), cast_single(right, right_type)};
}

IRGenerator::CodegenValue IRGenerator::generate_binary_op(CodegenValue left, BinaryOp op,
                                                          CodegenValue right) {
  const auto one_pointer = left.get_type().is_pointer() ^ right.get_type().is_pointer();

  if (one_pointer && (op == BinaryOp::Add || op == BinaryOp::Sub)) {
    if (op == BinaryOp::Add && right.get_type().is_pointer()) {
      std::swap(left, right);
    }

    verify(left.get_type().is_nonvoid_pointer(), "Left operand must be non-void pointer");
    verify(right.get_type().is_arithmetic(), "Right operand must be arithmetic");

    const auto left_v = extract_value(left);
    auto right_v = implicit_cast(right, Type(Type::Kind::U64));

    if (op == BinaryOp::Sub) {
      right_v = inserter.neg(right_v);
    }

    return CodegenValue::rvalue(left.get_type(), inserter.offset(left_v, right_v));
  }

  switch (op) {
  case BinaryOp::Equal:
  case BinaryOp::NotEqual:
  case BinaryOp::Gt:
  case BinaryOp::Gte:
  case BinaryOp::Lt:
  case BinaryOp::Lte: {
    const auto [new_left, new_right] = implcit_cast_binary(left, right);
    const auto predicate = convert_to_ir_int_predicate(op, new_left.get_type().is_signed());

    const auto zero = context->get_i8_ty()->get_zero();
    const auto one = context->get_i8_ty()->get_one();

    const auto result_i1 =
      inserter.int_compare(extract_value(new_left), predicate, extract_value(new_right));
    const auto result = inserter.select(result_i1, one, zero);

    return CodegenValue::rvalue(Type(Type::Kind::U8), result);
  }

  default: {
    verify(left.get_type().is_arithmetic() && right.get_type().is_arithmetic(),
           "Binary operation operands must be arthmetic");

    const auto [new_left, new_right] = implcit_cast_binary(left, right);
    const auto ir_op = convert_to_ir_binary_op(op, new_left.get_type().is_signed());

    const auto result =
      inserter.binary_instr(extract_value(new_left), ir_op, extract_value(new_right));

    return CodegenValue::rvalue(new_left.get_type(), result);
  }
  }
}

fz::Value* IRGenerator::generate_condition(const Expr* condition) {
  const auto condition_v = generate_nonvoid_expression(condition);
  const auto zero = convert_type(condition_v.get_type())->get_zero();

  return inserter.int_compare(extract_value(condition_v), fz::IntPredicate::NotEqual, zero);
}

IRGenerator::VisitResult IRGenerator::visit_assign_stmt(Argument<AssignStmt> assign_stmt) {
  const auto variable = generate_nonvoid_expression(assign_stmt->get_variable());
  const auto value = generate_nonvoid_expression(assign_stmt->get_value());

  verify(variable.is_lvalue(), "Cannot assign to lvalue");

  inserter.store(variable.get_raw_value(), implicit_cast(value, variable.get_type()));

  return std::nullopt;
}

IRGenerator::VisitResult
IRGenerator::visit_binary_assign_stmt(Argument<BinaryAssignStmt> binary_assign_stmt) {
  const auto variable = generate_nonvoid_expression(binary_assign_stmt->get_variable());
  const auto value = generate_nonvoid_expression(binary_assign_stmt->get_value());

  verify(variable.is_lvalue(), "Cannot assign to lvalue");

  const auto result = generate_binary_op(variable, binary_assign_stmt->get_op(), value);
  inserter.store(variable.get_raw_value(), implicit_cast(result, variable.get_type()));

  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_declare_stmt(Argument<DeclareStmt> declare_stmt) {
  bool is_array = false;
  size_t size = 1;

  if (const auto& array_size = declare_stmt->get_array_size()) {
    const auto size_opt = evaluate_constant_expression(array_size.get());
    verify(size_opt, "Array size must be constant");

    is_array = true;
    size = *size_opt;
  }

  const auto& name = declare_stmt->get_name();

  const auto type = declare_stmt->get_type();
  const auto decl_type = declare_stmt->get_declaration_type();

  const auto variable = inserter.stack_alloc(convert_type(decl_type), size);

  if (const auto& value = declare_stmt->get_value()) {
    verify(!is_array, "Arrays cannot have initializers");
    inserter.store(variable, implicit_cast(generate_nonvoid_expression(value), type));
  }

  if (is_array) {
    variables.insert(name, CodegenValue::rvalue(type, variable));
  } else {
    variables.insert(name, CodegenValue::lvalue(type, variable));
  }

  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_while_stmt(Argument<WhileStmt> while_stmt) {
  const auto head = current_ir_function->create_block();
  const auto body = current_ir_function->create_block();
  const auto end = current_ir_function->create_block();

  inserter.branch(head);

  inserter.set_insertion_block(head);
  inserter.cond_branch(generate_condition(while_stmt->get_condition().get()), body, end);

  inserter.set_insertion_block(body);
  loops.push_back(Loop{head, end});
  if (!generate_body(while_stmt->get_body())) {
    inserter.branch(head);
  }
  loops.pop_back();

  inserter.set_insertion_block(end);

  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_if_stmt(Argument<IfStmt> if_stmt) {
  const auto end = current_ir_function->create_block();

  for (const auto& [condition, body] : if_stmt->get_arms()) {
    const auto on_true = current_ir_function->create_block();
    const auto on_false = current_ir_function->create_block();

    inserter.cond_branch(generate_condition(condition.get()), on_true, on_false);

    inserter.set_insertion_block(on_true);
    if (!generate_body(body)) {
      inserter.branch(end);
    }

    inserter.set_insertion_block(on_false);
  }

  if (const auto& default_body = if_stmt->get_default_body()) {
    if (!generate_body(default_body)) {
      inserter.branch(end);
    }
  } else {
    inserter.branch(end);
  }

  inserter.set_insertion_block(end);

  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_for_stmt(Argument<ForStmt> for_stmt) {
  const auto generate_statement_or_body = [this](const std::unique_ptr<Stmt>& stmt) -> bool {
    if (const auto body = cast<BodyStmt>(stmt.get())) {
      return generate_body(body);
    } else {
      generate_statement(stmt.get());
      return false;
    }
  };

  variables.enter_scope();

  if (const auto& init = for_stmt->get_init()) {
    verify(!generate_statement_or_body(init), "Terminating for init statement is disallowed");
  }

  const auto head = current_ir_function->create_block();
  const auto body = current_ir_function->create_block();
  const auto continue_ = current_ir_function->create_block();
  const auto end = current_ir_function->create_block();

  inserter.branch(head);

  inserter.set_insertion_block(head);
  if (const auto& condition = for_stmt->get_condition()) {
    inserter.cond_branch(generate_condition(condition.get()), body, end);
  } else {
    inserter.branch(body);
  }

  inserter.set_insertion_block(body);
  loops.push_back(Loop{continue_, end});
  if (!generate_body(for_stmt->get_body())) {
    inserter.branch(continue_);
  }
  loops.pop_back();

  inserter.set_insertion_block(continue_);
  if (const auto& step = for_stmt->get_step()) {
    verify(!generate_statement_or_body(step), "Terminating for step statement is disallowed");
  }
  inserter.branch(head);

  inserter.set_insertion_block(end);

  variables.exit_scope();

  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_return_stmt(Argument<ReturnStmt> return_stmt) {
  if (const auto& ret_value = return_stmt->get_return_value()) {
    const auto value = generate_nonvoid_expression(ret_value);
    const auto casted = implicit_cast(value, current_function->get_prototype().get_return_type());

    inserter.ret(casted);
  } else {
    verify(current_ir_function->get_return_type()->is_void(),
           "Cannot return void from non-void function");

    inserter.ret();
  }

  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_break_stmt(Argument<BreakStmt> break_stmt) {
  inserter.branch(get_current_loop().break_label);
  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_continue_stmt(Argument<ContinueStmt> continue_stmt) {
  inserter.branch(get_current_loop().continue_label);
  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_body_stmt(Argument<BodyStmt> body_stmt) {
  generate_body(body_stmt);
  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_variable_expr(Argument<VariableExpr> variable_expr) {
  return variables.get(variable_expr->get_name());
}

IRGenerator::VisitResult IRGenerator::visit_unary_expr(Argument<UnaryExpr> unary_expr) {
  const auto value = generate_nonvoid_expression(unary_expr->get_value());

  switch (unary_expr->get_op()) {
  case UnaryOp::Neg:
  case UnaryOp::Not: {
    verify(value.get_type().is_arithmetic(),
           "Unary operator can be only applied on arithmetic type");

    const auto ir_op = unary_expr->get_op() == UnaryOp::Neg ? fz::UnaryOp::Neg : fz::UnaryOp::Not;
    const auto result = inserter.unary_instr(ir_op, extract_value(value));

    return CodegenValue::rvalue(value.get_type(), result);
  }

  case UnaryOp::Ref: {
    verify(value.is_lvalue(), "Cannot get address of rvalue");
    return CodegenValue::rvalue(value.get_type().add_pointer(), value.get_raw_value());
  }

  case UnaryOp::Deref: {
    const auto new_type = value.get_type().strip_pointer();
    return CodegenValue::lvalue(new_type, extract_value(value));
  }

  default:
    unreachable();
  }
}

IRGenerator::VisitResult IRGenerator::visit_binary_expr(Argument<BinaryExpr> binary_expr) {
  const auto left = generate_nonvoid_expression(binary_expr->get_left());
  const auto right = generate_nonvoid_expression(binary_expr->get_right());

  return generate_binary_op(left, binary_expr->get_op(), right);
}

IRGenerator::VisitResult IRGenerator::visit_number_expr(Argument<NumberExpr> number_expr) {
  const auto type = number_expr->get_type();
  const auto constant = convert_type(type)->get_constant(number_expr->get_value());

  return CodegenValue::rvalue(type, constant);
}

IRGenerator::VisitResult IRGenerator::visit_array_expr(Argument<ArrayExpr> array_expr) {
  const auto array = generate_nonvoid_expression(array_expr->get_array());
  const auto index = generate_nonvoid_expression(array_expr->get_index());

  verify(array.get_type().is_nonvoid_pointer(), "Array must be non-void pointer");
  verify(index.get_type().is_arithmetic(), "Index must be arithmetic");

  const auto array_v = extract_value(array);
  const auto index_v = implicit_cast(index, Type(Type::Kind::U64));

  const auto type = array.get_type().strip_pointer();
  const auto result = inserter.offset(array_v, index_v);

  return CodegenValue::lvalue(type, result);
}

IRGenerator::VisitResult IRGenerator::visit_call_expr(Argument<CallExpr> call_expr) {
  const auto it = function_map.find(call_expr->get_function_name());
  verify(it != function_map.end(), "Called unknown function {}", call_expr->get_function_name());

  const auto& target_prototype = it->second->get_prototype();
  const auto target_ir_function = module->get_function(call_expr->get_function_name());

  std::vector<fz::Value*> arguments(call_expr->get_arguments().size());
  verify(arguments.size() == target_prototype.get_arguments().size(),
         "Function call argument count mismatch");

  for (size_t i = 0; i < arguments.size(); ++i) {
    const auto value = generate_nonvoid_expression(call_expr->get_arguments()[i]);
    const auto casted = implicit_cast(value, target_prototype.get_arguments()[i].first);

    arguments[i] = casted;
  }

  const auto result = inserter.call(target_ir_function, arguments);
  if (!result->is_void()) {
    return CodegenValue::rvalue(target_prototype.get_return_type(), result);
  }

  return std::nullopt;
}

IRGenerator::VisitResult IRGenerator::visit_cast_expr(Argument<CastExpr> cast_expr) {
  const auto value = generate_nonvoid_expression(cast_expr->get_value());
  const auto extracted = extract_value(value);

  const auto from_type = value.get_type();
  const auto to_type = cast_expr->get_type();

  const auto from_ir_type = convert_type(from_type);
  const auto to_ir_type = convert_type(to_type);

  fz::Value* result;
  if (from_ir_type == to_ir_type) {
    result = extracted;
  } else if (from_type.is_arithmetic() && to_type.is_arithmetic()) {
    result = int_cast(extracted, from_type, to_type);
  } else if (from_type.get_byte_size() == to_type.get_byte_size()) {
    result = inserter.bitcast(extracted, to_ir_type);
  } else {
    if (from_type.is_pointer()) {
      const auto x = inserter.bitcast(extracted, context->get_i64_ty());
      result = int_cast(x, Type(Type::Kind::U64), to_type);
    } else {
      const auto x = int_cast(extracted, from_type, Type(Type::Kind::U64));
      result = inserter.bitcast(x, to_ir_type);
    }
  }

  return CodegenValue::rvalue(to_type, result);
}

IRGenerator::CodegenValue IRGenerator::generate_nonvoid_expression(const Expr* expr) {
  const auto result = generate_expression(expr);
  verify(result, "Expected non-void expression, got void one");

  return *result;
}

std::optional<IRGenerator::CodegenValue> IRGenerator::generate_expression(const Expr* expr) {
  return visitor::visit_statement(expr, *this);
}

void IRGenerator::generate_statement(const Stmt* stmt) { visitor::visit_statement(stmt, *this); }

bool IRGenerator::generate_body(const BodyStmt* body) {
  variables.enter_scope();

  bool terminated = false;

  for (const auto& stmt : body->get_statements()) {
    generate_statement(stmt.get());

    if (inserter.get_insertion_block()->is_terminated()) {
      terminated = true;
      break;
    }
  }

  variables.exit_scope();

  return terminated;
}

void IRGenerator::generate_local_function(const Function& function, fz::Function* ir_function) {
  current_ir_function = ir_function;
  current_function = &function;
  variables.clear();
  loops.clear();

  const auto entry_block = ir_function->create_block();
  inserter.set_insertion_block(entry_block);

  variables.enter_scope();
  {
    size_t parameter_index = 0;
    for (const auto& [type, name] : function.get_prototype().get_arguments()) {
      const auto storage = inserter.stack_alloc(convert_type(type));
      const auto parameter = ir_function->get_parameter(parameter_index++);
      inserter.store(storage, parameter);

      variables.insert(name, CodegenValue::lvalue(type, storage));
    }

    if (!generate_body(function.get_body())) {
      if (ir_function->get_return_type()->is_void()) {
        inserter.ret();
      } else {
        fatal_error("No return statement in non-void function.");
      }
    }
  }
  variables.exit_scope();

  current_ir_function = nullptr;
  current_function = nullptr;
}

void IRGenerator::create_declarations(const std::vector<Function>& functions) {
  for (const Function& function : functions) {
    const auto& prototype = function.get_prototype();

    std::vector<fz::Type*> arguments;
    for (const auto& [type, name] : prototype.get_arguments()) {
      arguments.push_back(convert_type(type));
    }

    const auto& name = prototype.get_name();
    const auto ir_function =
      module->create_function(convert_type(prototype.get_return_type()), name, arguments);

    verify(function_map.insert({name, &function}).second, "Defined multiple functions named {}.",
           name);
  }
}

void IRGenerator::generate_ir_for_functions(const std::vector<Function>& functions) {
  function_map.clear();

  create_declarations(functions);

  for (const Function& function : functions) {
    if (function.get_body()) {
      const auto& name = function.get_prototype().get_name();
      generate_local_function(function, module->get_function(name));
    }
  }
}

IRGenerator::IRGenerator(fz::Context* context)
    : context(context), module(context->create_module()) {}

fz::Module* IRGenerator::generate(fz::Context* context, const std::vector<Function>& functions) {
  IRGenerator generator(context);
  generator.generate_ir_for_functions(functions);

  return generator.module;
}