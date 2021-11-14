#pragma once
#include "Block.hpp"
#include "Instruction.hpp"

namespace flugzeug {

class Function;

enum class UnaryOp {
  Neg,
  Not,
};

enum class BinaryOp {
  Add,
  Sub,
  Mul,
  ModU,
  DivU,
  ModS,
  DivS,
  Shr,
  Shl,
  Sar,
  And,
  Or,
  Xor,
};

enum class IntPredicate {
  Equal,
  NotEqual,

  GtU,
  GteU,
  GtS,
  GteS,

  LtU,
  LteU,
  LtS,
  LteS,
};

enum class CastKind {
  ZeroExtend,
  SignExtend,
  Truncate,
  Bitcast,
};

class UnaryInstr : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::UnaryInstr)

  UnaryOp op;

public:
  UnaryInstr(Context* context, UnaryOp op, Value* val)
      : Instruction(context, Value::Kind::UnaryInstr, val->get_type()), op(op) {
    set_operand_count(1);
    set_val(val);
  }

  UnaryOp get_op() const { return op; }
  bool is(UnaryOp other) const { return op == other; }

  Value* get_val() { return get_operand(0); }
  const Value* get_val() const { return get_operand(0); }

  void set_val(Value* val) { return set_operand(0, val); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class BinaryInstr : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::BinaryInstr)

  BinaryOp op;

public:
  BinaryInstr(Context* context, Value* lhs, BinaryOp op, Value* rhs)
      : Instruction(context, Value::Kind::BinaryInstr, lhs->get_type()), op(op) {
    set_operand_count(2);
    set_lhs(lhs);
    set_rhs(rhs);
  }

  BinaryOp get_op() const { return op; }
  bool is(BinaryOp other) const { return op == other; }

  Value* get_lhs() { return get_operand(0); }
  const Value* get_lhs() const { return get_operand(0); }

  Value* get_rhs() { return get_operand(1); }
  const Value* get_rhs() const { return get_operand(1); }

  void set_lhs(Value* lhs) { return set_operand(0, lhs); }
  void set_rhs(Value* rhs) { return set_operand(1, rhs); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class IntCompare : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::IntCompare)

  IntPredicate pred;

public:
  IntCompare(Context* context, Value* lhs, IntPredicate pred, Value* rhs)
      : Instruction(context, Value::Kind::IntCompare, context->get_i1_ty()), pred(pred) {
    set_operand_count(2);
    set_lhs(lhs);
    set_rhs(rhs);
  }

  IntPredicate get_pred() const { return pred; }
  bool is(IntPredicate other) const { return pred == other; }

  Value* get_lhs() { return get_operand(0); }
  const Value* get_lhs() const { return get_operand(0); }

  Value* get_rhs() { return get_operand(1); }
  const Value* get_rhs() const { return get_operand(1); }

  void set_lhs(Value* lhs) { return set_operand(0, lhs); }
  void set_rhs(Value* rhs) { return set_operand(1, rhs); }

  static IntPredicate inverted_predicate(IntPredicate pred);

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Load : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Load)

public:
  explicit Load(Context* context, Value* ptr)
      : Instruction(context, Value::Kind::Load, cast<PointerType>(ptr->get_type())->deref()) {
    set_operand_count(1);
    set_ptr(ptr);
  }

  Value* get_ptr() { return get_operand(0); }
  const Value* get_ptr() const { return get_operand(0); }

  void set_ptr(Value* ptr) { return set_operand(0, ptr); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Store : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Store)

public:
  Store(Context* context, Value* ptr, Value* val)
      : Instruction(context, Value::Kind::Store, context->get_void_ty()) {
    set_operand_count(2);
    set_ptr(ptr);
    set_val(val);
  }

  Value* get_ptr() { return get_operand(0); }
  const Value* get_ptr() const { return get_operand(0); }

  Value* get_val() { return get_operand(1); }
  const Value* get_val() const { return get_operand(1); }

  void set_ptr(Value* ptr) { return set_operand(0, ptr); }
  void set_val(Value* val) { return set_operand(1, val); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Call : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Call)

  Function* target = nullptr;

public:
  Call(Context* context, Function* function, const std::vector<Value*>& arguments);

  size_t get_arg_count() const { return get_operand_count(); }

  Value* get_arg(size_t i) { return get_operand(i); }
  const Value* get_arg(size_t i) const { return get_operand(i); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Branch : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Branch)

public:
  explicit Branch(Context* context, Block* target)
      : Instruction(context, Value::Kind::Branch, context->get_void_ty()) {
    set_operand_count(1);
    set_target(target);
  }

  Block* get_target() { return cast<Block>(get_operand(0)); }
  const Block* get_target() const { return cast<Block>(get_operand(0)); }

  void set_target(Block* target) { set_operand(0, target); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class CondBranch : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::CondBranch)

public:
  explicit CondBranch(Context* context, Value* cond, Block* true_target, Block* false_target)
      : Instruction(context, Value::Kind::CondBranch, context->get_void_ty()) {
    set_operand_count(3);
    set_cond(cond);
    set_true_target(true_target);
    set_false_target(false_target);
  }

  Value* get_cond() { return get_operand(0); }
  const Value* get_cond() const { return get_operand(0); }

  Block* get_true_target() { return cast<Block>(get_operand(1)); }
  const Block* get_true_target() const { return cast<Block>(get_operand(1)); }

  Block* get_false_target() { return cast<Block>(get_operand(2)); }
  const Block* get_false_target() const { return cast<Block>(get_operand(2)); }

  Block* get_target(bool b) { return b ? get_true_target() : get_false_target(); }
  const Block* get_target(bool b) const { return b ? get_true_target() : get_false_target(); }

  void set_cond(Value* cond) { set_operand(0, cond); }
  void set_true_target(Block* true_target) { set_operand(1, true_target); }
  void set_false_target(Block* false_target) { set_operand(2, false_target); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class StackAlloc : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::StackAlloc)

  size_t size;

public:
  explicit StackAlloc(Context* context, Type* type, size_t size = 1)
      : Instruction(context, Value::Kind::StackAlloc, type->ref()), size(size) {}

  size_t get_size() const { return size; }
  Type* get_allocated_type() const { return cast<PointerType>(get_type())->deref(); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Ret : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Ret)

public:
  explicit Ret(Context* context, Value* val = nullptr)
      : Instruction(context, Value::Kind::Ret, context->get_void_ty()) {
    if (val) {
      set_operand_count(1);
      set_val(val);
    }
  }

  bool is_ret_void() const { return get_operand_count() == 0; }

  Value* get_val() { return is_ret_void() ? nullptr : get_operand(0); }
  const Value* get_val() const { return is_ret_void() ? nullptr : get_operand(0); }

  void set_val(Value* val) {
    verify(!is_ret_void(), "Cannot set value for ret void.");
    return set_operand(0, val);
  }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Offset : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Offset)

public:
  Offset(Context* context, Value* base, Value* index)
      : Instruction(context, Value::Kind::Offset, base->get_type()) {
    set_operand_count(2);
    set_base(base);
    set_index(index);
  }

  Value* get_base() { return get_operand(0); }
  const Value* get_base() const { return get_operand(0); }

  Value* get_index() { return get_operand(1); }
  const Value* get_index() const { return get_operand(1); }

  void set_base(Value* base) { return set_operand(0, base); }
  void set_index(Value* index) { return set_operand(1, index); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Cast : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Cast)

  CastKind cast_kind;

public:
  Cast(Context* context, Value* val, CastKind cast_kind, Type* target_type)
      : Instruction(context, Value::Kind::Cast, target_type), cast_kind(cast_kind) {
    set_operand_count(1);
    set_val(val);
  }

  CastKind get_cast_kind() const { return cast_kind; }
  bool is(CastKind other) const { return cast_kind == other; }

  Value* get_val() { return get_operand(0); }
  const Value* get_val() const { return get_operand(0); }

  void set_val(Value* val) { return set_operand(0, val); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Select : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Select)

public:
  Select(Context* context, Value* cond, Value* true_val, Value* false_val)
      : Instruction(context, Value::Kind::Select, true_val->get_type()) {
    set_operand_count(3);
    set_cond(cond);
    set_true_val(true_val);
    set_false_val(false_val);
  }

  Value* get_cond() { return get_operand(0); }
  const Value* get_cond() const { return get_operand(0); }

  Value* get_true_val() { return get_operand(1); }
  const Value* get_true_val() const { return get_operand(1); }

  Value* get_false_val() { return get_operand(2); }
  const Value* get_false_val() const { return get_operand(2); }

  Value* get_val(bool b) { return b ? get_true_val() : get_false_val(); }
  const Value* get_val(bool b) const { return b ? get_true_val() : get_false_val(); }

  void set_cond(Value* cond) { return set_operand(0, cond); }
  void set_true_val(Value* true_val) { return set_operand(1, true_val); }
  void set_false_val(Value* false_val) { return set_operand(2, false_val); }

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

class Phi : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Phi)

  static inline size_t get_block_index(size_t i) { return i * 2 + 0; }
  static inline size_t get_value_index(size_t i) { return i * 2 + 1; }

  bool index_for_block(const Block* block, size_t& index) const;
  void remove_incoming_by_index(size_t index);

public:
  struct Incoming {
    Block* block;
    Value* value;
  };

  struct ConstIncoming {
    const Block* block;
    const Value* value;
  };

  explicit Phi(Context* context, Type* type) : Instruction(context, Instruction::Kind::Phi, type) {}
  explicit Phi(Context* context, const std::vector<Incoming>& incoming)
      : Phi(context, incoming[0].value->get_type()) {
    reserve_operands(incoming.size());

    for (const auto& i : incoming) {
      add_incoming(i);
    }
  }

  size_t get_incoming_count() const { return get_operand_count() / 2; }
  bool is_empty() const { return get_operand_count() == 0; }

  Incoming get_incoming(size_t i) {
    return Incoming{cast<Block>(get_operand(get_block_index(i))), get_operand(get_value_index(i))};
  }
  ConstIncoming get_incoming(size_t i) const {
    return ConstIncoming{cast<Block>(get_operand(get_block_index(i))),
                         get_operand(get_value_index(i))};
  }

  Value* get_incoming_value(size_t i) { return get_operand(get_value_index(i)); }
  const Value* get_incoming_value(size_t i) const { return get_operand(get_value_index(i)); }

  Value* get_single_incoming_value();

  bool remove_incoming_opt(const Block* block);
  void remove_incoming(const Block* block);
  void add_incoming(Block* block, Value* value);
  void add_incoming(const Incoming& incoming) { add_incoming(incoming.block, incoming.value); }

  bool replace_incoming_block_opt(const Block* old_incoming, Block* new_incoming);
  void replace_incoming_block(const Block* old_incoming, Block* new_incoming);

  Value* get_incoming_by_block(const Block* block);
  const Value* get_incoming_by_block(const Block* block) const;

protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
};

} // namespace flugzeug