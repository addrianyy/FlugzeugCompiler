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

  UnaryOp op_;

 public:
  UnaryInstr(Context* context, UnaryOp op, Value* value)
      : Instruction(context, Value::Kind::UnaryInstr, value->type()), op_(op) {
    set_operand_count(1);
    set_value(value);
  }

  UnaryOp op() const { return op_; }
  bool is(UnaryOp other) const { return op_ == other; }

  Value* value() { return operand(0); }
  const Value* value() const { return operand(0); }

  void set_value(Value* value) { return set_operand(0, value); }
  void set_op(UnaryOp new_op) { op_ = new_op; }

  void set_new_operands(UnaryOp new_op, Value* value) {
    set_op(new_op);
    set_value(value);
  }

  Instruction* clone() override { return new UnaryInstr(context(), op(), value()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class BinaryInstr final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::BinaryInstr)

  BinaryOp op_;

 public:
  BinaryInstr(Context* context, Value* lhs, BinaryOp op, Value* rhs)
      : Instruction(context, Value::Kind::BinaryInstr, lhs->type()), op_(op) {
    set_operand_count(2);
    set_lhs(lhs);
    set_rhs(rhs);
  }

  BinaryOp op() const { return op_; }
  bool is(BinaryOp other) const { return op_ == other; }

  Value* lhs() { return operand(0); }
  const Value* lhs() const { return operand(0); }

  Value* rhs() { return operand(1); }
  const Value* rhs() const { return operand(1); }

  void set_lhs(Value* lhs) { return set_operand(0, lhs); }
  void set_rhs(Value* rhs) { return set_operand(1, rhs); }
  void set_op(BinaryOp new_op) { op_ = new_op; }

  void set_new_operands(Value* lhs, BinaryOp new_op, Value* rhs) {
    set_lhs(lhs);
    set_op(new_op);
    set_rhs(rhs);
  }

  Instruction* clone() override { return new BinaryInstr(context(), lhs(), op(), rhs()); }

  static bool is_binary_op_commutative(BinaryOp op);

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class IntCompare final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::IntCompare)

  IntPredicate predicate_;

 public:
  IntCompare(Context* context, Value* lhs, IntPredicate predicate, Value* rhs)
      : Instruction(context, Value::Kind::IntCompare, context->i1_ty()), predicate_(predicate) {
    set_operand_count(2);
    set_lhs(lhs);
    set_rhs(rhs);
  }

  IntPredicate predicate() const { return predicate_; }
  bool is(IntPredicate other) const { return predicate_ == other; }

  Value* lhs() { return operand(0); }
  const Value* lhs() const { return operand(0); }

  Value* rhs() { return operand(1); }
  const Value* rhs() const { return operand(1); }

  void set_lhs(Value* lhs) { return set_operand(0, lhs); }
  void set_rhs(Value* rhs) { return set_operand(1, rhs); }
  void set_predicate(IntPredicate new_predicate) { predicate_ = new_predicate; }

  void set_new_operands(Value* lhs, IntPredicate new_predicate, Value* rhs) {
    set_lhs(lhs);
    set_predicate(new_predicate);
    set_rhs(rhs);
  }

  Instruction* clone() override { return new IntCompare(context(), lhs(), predicate(), rhs()); }

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
  explicit Load(Context* context, Value* address)
      : Instruction(context, Value::Kind::Load, cast<PointerType>(address->type())->deref()) {
    set_operand_count(1);
    set_address(address);
  }

  Value* address() { return operand(0); }
  const Value* address() const { return operand(0); }

  void set_address(Value* address) { return set_operand(0, address); }

  void set_new_operands(Value* address) { set_address(address); }

  Instruction* clone() override { return new Load(context(), address()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Store final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Store)

 public:
  Store(Context* context, Value* address, Value* stored_value)
      : Instruction(context, Value::Kind::Store, context->void_ty()) {
    set_operand_count(2);
    set_address(address);
    set_stored_value(stored_value);
  }

  Value* address() { return operand(0); }
  const Value* address() const { return operand(0); }

  Value* value() { return operand(1); }
  const Value* value() const { return operand(1); }

  void set_address(Value* address) { return set_operand(0, address); }
  void set_stored_value(Value* stored_value) { return set_operand(1, stored_value); }

  void set_new_operands(Value* address, Value* stored_value) {
    set_address(address);
    set_stored_value(stored_value);
  }

  Instruction* clone() override { return new Store(context(), address(), value()); }

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

  size_t argument_count() const { return operand_count() - 1; }

  Value* argument(size_t i) { return operand(i + 1); }
  const Value* argument(size_t i) const { return operand(i + 1); }

  Function* callee();
  const Function* callee() const;

  Instruction* clone() override {
    std::vector<Value*> arguments;
    arguments.reserve(argument_count());
    for (size_t i = 0; i < argument_count(); ++i) {
      arguments.push_back(argument(i));
    }
    return new Call(context(), callee(), arguments);
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
      : Instruction(context, Value::Kind::Branch, context->void_ty()) {
    set_operand_count(1);
    set_target(target);
  }

  Block* target() { return cast<Block>(operand(0)); }
  const Block* target() const { return cast<Block>(operand(0)); }

  void set_target(Block* target) { set_operand(0, target); }

  void set_new_operands(Block* target) { set_target(target); }

  Instruction* clone() override { return new Branch(context(), target()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class CondBranch final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::CondBranch)

 public:
  explicit CondBranch(Context* context, Value* condition, Block* true_target, Block* false_target)
      : Instruction(context, Value::Kind::CondBranch, context->void_ty()) {
    set_operand_count(3);
    set_condition(condition);
    set_true_target(true_target);
    set_false_target(false_target);
  }

  Value* condition() { return operand(0); }
  const Value* condition() const { return operand(0); }

  Block* true_target() { return cast<Block>(operand(1)); }
  const Block* true_target() const { return cast<Block>(operand(1)); }

  Block* false_target() { return cast<Block>(operand(2)); }
  const Block* false_target() const { return cast<Block>(operand(2)); }

  Block* select_target(bool b) { return b ? true_target() : false_target(); }
  const Block* select_target(bool b) const { return b ? true_target() : false_target(); }

  void set_condition(Value* condition) { set_operand(0, condition); }
  void set_true_target(Block* true_target) { set_operand(1, true_target); }
  void set_false_target(Block* false_target) { set_operand(2, false_target); }

  void set_new_operands(Value* condition, Block* true_target, Block* false_target) {
    set_condition(condition);
    set_true_target(true_target);
    set_false_target(false_target);
  }

  Instruction* clone() override {
    return new CondBranch(context(), condition(), true_target(), false_target());
  }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class StackAlloc final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::StackAlloc)

  size_t size_;

 public:
  explicit StackAlloc(Context* context, Type* type, size_t size = 1)
      : Instruction(context, Value::Kind::StackAlloc, type->ref()), size_(size) {}

  bool is_scalar() const { return size_ == 1; }

  size_t size() const { return size_; }
  Type* allocated_type() const { return cast<PointerType>(type())->deref(); }

  Instruction* clone() override { return new StackAlloc(context(), allocated_type(), size()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Ret final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Ret)

 public:
  explicit Ret(Context* context, Value* return_value = nullptr)
      : Instruction(context, Value::Kind::Ret, context->void_ty()) {
    if (return_value) {
      set_operand_count(1);
      set_return_value(return_value);
    }
  }

  bool returns_void() const { return operand_count() == 0; }

  Value* return_value() { return returns_void() ? nullptr : operand(0); }
  const Value* return_value() const { return returns_void() ? nullptr : operand(0); }

  void set_return_value(Value* return_value) {
    verify(!returns_void(), "Cannot set value for ret void.");
    return set_operand(0, return_value);
  }

  void set_new_operands(Value* return_value) { set_return_value(return_value); }

  Instruction* clone() override { return new Ret(context(), return_value()); }

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
      : Instruction(context, Value::Kind::Offset, base->type()) {
    set_operand_count(2);
    set_base(base);
    set_index(index);
  }

  Value* base() { return operand(0); }
  const Value* base() const { return operand(0); }

  Value* index() { return operand(1); }
  const Value* index() const { return operand(1); }

  void set_base(Value* base) { return set_operand(0, base); }
  void set_index(Value* index) { return set_operand(1, index); }

  void set_new_operands(Value* base, Value* index) {
    set_base(base);
    set_index(index);
  }

  Instruction* clone() override { return new Offset(context(), base(), index()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Cast final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Cast)

  CastKind cast_kind_;

 public:
  Cast(Context* context, CastKind cast_kind, Value* casted_value, Type* target_type)
      : Instruction(context, Value::Kind::Cast, target_type), cast_kind_(cast_kind) {
    set_operand_count(1);
    set_casted_value(casted_value);
  }

  CastKind cast_kind() const { return cast_kind_; }
  bool is(CastKind other) const { return cast_kind_ == other; }

  Value* casted_value() { return operand(0); }
  const Value* casted_value() const { return operand(0); }

  void set_casted_value(Value* casted_value) { return set_operand(0, casted_value); }
  void set_cast_kind(CastKind new_cast_kind) { cast_kind_ = new_cast_kind; }

  Instruction* clone() override { return new Cast(context(), cast_kind(), casted_value(), type()); }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

class Select final : public Instruction {
  DEFINE_INSTANCEOF(Value, Value::Kind::Select)

 public:
  Select(Context* context, Value* condition, Value* true_value, Value* false_value)
      : Instruction(context, Value::Kind::Select, true_value->type()) {
    set_operand_count(3);
    set_condition(condition);
    set_true_value(true_value);
    set_false_value(false_value);
  }

  Value* condition() { return operand(0); }
  const Value* condition() const { return operand(0); }

  Value* true_value() { return operand(1); }
  const Value* true_value() const { return operand(1); }

  Value* false_value() { return operand(2); }
  const Value* false_value() const { return operand(2); }

  Value* select_value(bool b) { return b ? true_value() : false_value(); }
  const Value* select_value(bool b) const { return b ? true_value() : false_value(); }

  void set_condition(Value* condition) { return set_operand(0, condition); }
  void set_true_value(Value* true_value) { return set_operand(1, true_value); }
  void set_false_value(Value* false_value) { return set_operand(2, false_value); }

  void set_new_operands(Value* condition, Value* true_value, Value* false_value) {
    set_condition(condition);
    set_true_value(true_value);
    set_false_value(false_value);
  }

  Instruction* clone() override {
    return new Select(context(), condition(), true_value(), false_value());
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
      if (incoming_index < phi->incoming_count()) {
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
    const auto value = operand(get_value_index(i));
    const auto block = cast<Block>(operand(get_block_index(i)));

    return Incoming{block, value};
  }
  ConstIncoming get_incoming(size_t i) const {
    const auto value = operand(get_value_index(i));
    const auto block = cast<Block>(operand(get_block_index(i)));

    return ConstIncoming{block, value};
  }

  Value* get_incoming_value(size_t i) { return operand(get_value_index(i)); }
  const Value* get_incoming_value(size_t i) const { return operand(get_value_index(i)); }

 public:
  explicit Phi(Context* context, Type* type) : Instruction(context, Instruction::Kind::Phi, type) {}
  explicit Phi(Context* context, const std::vector<Incoming>& incoming)
      : Phi(context, incoming[0].value->type()) {
    reserve_operands(incoming.size() * 2);

    for (const auto& i : incoming) {
      add_incoming(i);
    }
  }

  size_t incoming_count() const { return operand_count() / 2; }
  bool empty() const { return operand_count() == 0; }

  Value* single_incoming_value();

  Value* remove_incoming_opt(const Block* block);
  Value* remove_incoming(const Block* block);
  void add_incoming(Block* block, Value* value);
  void add_incoming(const Incoming& incoming) { add_incoming(incoming.block, incoming.value); }

  void replace_incoming_for_block(const Block* block, Value* new_incoming);

  bool replace_incoming_block_opt(const Block* old_incoming, Block* new_incoming);
  void replace_incoming_block(const Block* old_incoming, Block* new_incoming);

  Value* incoming_for_block(const Block* block);
  const Value* incoming_for_block(const Block* block) const;

  using const_iterator = IncomingIteratorInternal<const Phi, ConstIncoming>;
  using iterator = IncomingIteratorInternal<Phi, Incoming>;

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, incoming_count()); }

  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, incoming_count()); }

  Instruction* clone() override {
    std::vector<Incoming> incoming;
    incoming.reserve(incoming_count());
    for (size_t i = 0; i < incoming_count(); ++i) {
      incoming.push_back(get_incoming(i));
    }
    return new Phi(context(), incoming);
  }

 protected:
  void print_instruction_internal(IRPrinter::LinePrinter& printer) const override;
  void print_instruction_compact_internal(
    IRPrinter::LinePrinter& printer,
    const std::unordered_set<const Value*>& inlined_values) const override;
};

}  // namespace flugzeug