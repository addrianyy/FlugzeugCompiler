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

class UnaryInstr final : public Instruction {
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
  void set_op(UnaryOp new_op) { op = new_op; }

  void set_new_operands(UnaryOp new_op, Value* val) {
    set_op(new_op);
    set_val(val);
  }

  Instruction* clone() override { return new UnaryInstr(get_context(), get_op(), get_val()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class BinaryInstr final : public Instruction {
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
  void set_op(BinaryOp new_op) { op = new_op; }

  void set_new_operands(Value* lhs, BinaryOp new_op, Value* rhs) {
    set_lhs(lhs);
    set_op(new_op);
    set_rhs(rhs);
  }

  Instruction* clone() override {
    return new BinaryInstr(get_context(), get_lhs(), get_op(), get_rhs());
  }

  static bool is_binary_op_commutative(BinaryOp op);

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class IntCompare final : public Instruction {
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
  void set_pred(IntPredicate new_pred) { pred = new_pred; }

  void set_new_operands(Value* lhs, IntPredicate new_pred, Value* rhs) {
    set_lhs(lhs);
    set_pred(new_pred);
    set_rhs(rhs);
  }

  Instruction* clone() override {
    return new IntCompare(get_context(), get_lhs(), get_pred(), get_rhs());
  }

  static IntPredicate inverted_predicate(IntPredicate pred);
  static IntPredicate swapped_order_predicate(IntPredicate pred);

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Load final : public Instruction {
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

  void set_new_operands(Value* ptr) { set_ptr(ptr); }

  Instruction* clone() override { return new Load(get_context(), get_ptr()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Store final : public Instruction {
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

  void set_new_operands(Value* ptr, Value* val) {
    set_ptr(ptr);
    set_val(val);
  }

  Instruction* clone() override { return new Store(get_context(), get_ptr(), get_val()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Call final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Call)

 public:
  Call(Context* context, Function* function, const std::vector<Value*>& arguments);

  size_t get_arg_count() const { return get_operand_count() - 1; }

  Value* get_arg(size_t i) { return get_operand(i + 1); }
  const Value* get_arg(size_t i) const { return get_operand(i + 1); }

  Function* get_callee();
  const Function* get_callee() const;

  Instruction* clone() override {
    std::vector<Value*> arguments;
    arguments.reserve(get_arg_count());
    for (size_t i = 0; i < get_arg_count(); ++i) {
      arguments.push_back(get_arg(i));
    }
    return new Call(get_context(), get_callee(), arguments);
  }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Branch final : public Instruction {
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

  void set_new_operands(Block* target) { set_target(target); }

  Instruction* clone() override { return new Branch(get_context(), get_target()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class CondBranch final : public Instruction {
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

  void set_new_operands(Value* cond, Block* true_target, Block* false_target) {
    set_cond(cond);
    set_true_target(true_target);
    set_false_target(false_target);
  }

  Instruction* clone() override {
    return new CondBranch(get_context(), get_cond(), get_true_target(), get_false_target());
  }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class StackAlloc final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::StackAlloc)

  size_t size;

 public:
  explicit StackAlloc(Context* context, Type* type, size_t size = 1)
      : Instruction(context, Value::Kind::StackAlloc, type->ref()), size(size) {}

  size_t get_size() const { return size; }
  Type* get_allocated_type() const { return cast<PointerType>(get_type())->deref(); }

  Instruction* clone() override {
    return new StackAlloc(get_context(), get_allocated_type(), get_size());
  }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Ret final : public Instruction {
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

  void set_new_operands(Value* val) { set_val(val); }

  Instruction* clone() override { return new Ret(get_context(), get_val()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Offset final : public Instruction {
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

  void set_new_operands(Value* base, Value* index) {
    set_base(base);
    set_index(index);
  }

  Instruction* clone() override { return new Offset(get_context(), get_base(), get_index()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Cast final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Cast)

  CastKind cast_kind;

 public:
  Cast(Context* context, CastKind cast_kind, Value* val, Type* target_type)
      : Instruction(context, Value::Kind::Cast, target_type), cast_kind(cast_kind) {
    set_operand_count(1);
    set_val(val);
  }

  CastKind get_cast_kind() const { return cast_kind; }
  bool is(CastKind other) const { return cast_kind == other; }

  Value* get_val() { return get_operand(0); }
  const Value* get_val() const { return get_operand(0); }

  void set_val(Value* val) { return set_operand(0, val); }
  void set_cast_kind(CastKind new_cast_kind) { cast_kind = new_cast_kind; }

  Instruction* clone() override {
    return new Cast(get_context(), get_cast_kind(), get_val(), get_type());
  }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Select final : public Instruction {
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

  void set_new_operands(Value* cond, Value* true_val, Value* false_val) {
    set_cond(cond);
    set_true_val(false_val);
    set_false_val(false_val);
  }

  Instruction* clone() override {
    return new Select(get_context(), get_cond(), get_true_val(), get_false_val());
  }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Phi final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Phi)

 public:
  struct Incoming {
    Block* block;
    Value* value;
  };

  struct ConstIncoming {
    const Block* block;
    const Value* value;
  };

 private:
  static inline size_t get_block_index(size_t i) { return i * 2 + 0; }
  static inline size_t get_value_index(size_t i) { return i * 2 + 1; }

  template <typename TPhi, typename TIncoming>
  class IncomingIteratorInternal {
    TPhi* phi;
    TIncoming incoming;
    size_t incoming_index = 0;

    void load_incoming() {
      if (incoming_index < phi->get_incoming_count()) {
        incoming = phi->get_incoming(incoming_index);
      } else {
        incoming = TIncoming{};
      }
    }

   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = TIncoming;

    explicit IncomingIteratorInternal(TPhi* phi, size_t index) : phi(phi), incoming_index(index) {
      load_incoming();
    }

    IncomingIteratorInternal& operator++() {
      ++incoming_index;
      load_incoming();
      return *this;
    }

    IncomingIteratorInternal operator++(int) {
      const auto before = *this;
      ++(*this);
      return before;
    }

    value_type operator*() const { return incoming; }

    bool operator==(const IncomingIteratorInternal& rhs) const {
      return phi == rhs.phi && incoming_index == rhs.incoming_index;
    }
    bool operator!=(const IncomingIteratorInternal& rhs) const { return !(*this == rhs); }
  };

  bool index_for_block(const Block* block, size_t& index) const;
  void remove_incoming_by_index(size_t index);

  Incoming get_incoming(size_t i) {
    const auto value = get_operand(get_value_index(i));
    const auto block = cast<Block>(get_operand(get_block_index(i)));

    return Incoming{block, value};
  }
  ConstIncoming get_incoming(size_t i) const {
    const auto value = get_operand(get_value_index(i));
    const auto block = cast<Block>(get_operand(get_block_index(i)));

    return ConstIncoming{block, value};
  }

  Value* get_incoming_value(size_t i) { return get_operand(get_value_index(i)); }
  const Value* get_incoming_value(size_t i) const { return get_operand(get_value_index(i)); }

 public:
  explicit Phi(Context* context, Type* type) : Instruction(context, Instruction::Kind::Phi, type) {}
  explicit Phi(Context* context, const std::vector<Incoming>& incoming)
      : Phi(context, incoming[0].value->get_type()) {
    reserve_operands(incoming.size() * 2);

    for (const auto& i : incoming) {
      add_incoming(i);
    }
  }

  size_t get_incoming_count() const { return get_operand_count() / 2; }
  bool is_empty() const { return get_operand_count() == 0; }

  Value* get_single_incoming_value();

  Value* remove_incoming_opt(const Block* block);
  Value* remove_incoming(const Block* block);
  void add_incoming(Block* block, Value* value);
  void add_incoming(const Incoming& incoming) { add_incoming(incoming.block, incoming.value); }

  void replace_incoming_for_block(const Block* block, Value* new_incoming);

  bool replace_incoming_block_opt(const Block* old_incoming, Block* new_incoming);
  void replace_incoming_block(const Block* old_incoming, Block* new_incoming);

  Value* get_incoming_by_block(const Block* block);
  const Value* get_incoming_by_block(const Block* block) const;

  using const_iterator = IncomingIteratorInternal<const Phi, ConstIncoming>;
  using iterator = IncomingIteratorInternal<Phi, Incoming>;

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, get_incoming_count()); }

  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, get_incoming_count()); }

  Instruction* clone() override {
    std::vector<Incoming> incoming;
    incoming.reserve(get_incoming_count());
    for (size_t i = 0; i < get_incoming_count(); ++i) {
      incoming.push_back(get_incoming(i));
    }
    return new Phi(get_context(), incoming);
  }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

}  // namespace flugzeug