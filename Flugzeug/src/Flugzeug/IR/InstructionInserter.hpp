#pragma once
#include "Instructions.hpp"

namespace flugzeug {

class Instruction;

enum class InsertDestination {
  Front,
  Back,
};

class InstructionInserter {
  enum class InsertType {
    BlockFront,
    BlockBack,
    BeforeInstruction,
    AfterInstruction,
  };

  InsertType insert_type = InsertType::BlockBack;

  Block* insertion_block_ = nullptr;
  Instruction* insertion_instruction_ = nullptr;
  bool follow_instruction_ = false;

  Context* context = nullptr;

  void insert_internal(Instruction* instruction);

  template <typename T>
  T* insert(T* instruction) {
    insert_internal(instruction);
    return instruction;
  }

 public:
  InstructionInserter() = default;

  explicit InstructionInserter(Block* insertion_block,
                               InsertDestination destination = InsertDestination::Back) {
    set_insertion_block(insertion_block, destination);
  }

  explicit InstructionInserter(Instruction* insertion_instruction,
                               InsertDestination destination = InsertDestination::Back,
                               bool follow_instruction_ = true) {
    set_insertion_instruction(insertion_instruction, destination, follow_instruction_);
  }

  void set_insertion_block(Block* insertion_block,
                           InsertDestination destination = InsertDestination::Back);
  void set_insertion_instruction(Instruction* insertion_instruction,
                                 InsertDestination destination = InsertDestination::Back,
                                 bool follow_instruction = true);

  Block* insertion_block();

  UnaryInstr* unary_instr(UnaryOp op, Value* value);
  BinaryInstr* binary_instr(Value* lhs, BinaryOp op, Value* rhs);
  IntCompare* int_compare(Value* lhs, IntPredicate predicate, Value* rhs);
  Load* load(Value* address);
  Store* store(Value* address, Value* stored_value);
  Call* call(Function* function, const std::vector<Value*>& arguments);
  Branch* branch(Block* target);
  CondBranch* cond_branch(Value* condition, Block* true_target, Block* false_target);
  StackAlloc* stack_alloc(Type* type, size_t size = 1);
  Ret* ret(Value* value = nullptr);
  Offset* offset(Value* base, Value* index);
  Cast* cast(CastKind kind, Value* casted_value, Type* target_type);
  Select* select(Value* condition, Value* true_value, Value* false_value);
  Phi* phi(Type* type);
  Phi* phi(const std::vector<Phi::Incoming>& incoming);

#define UNARY_INSTR(name, op) \
  UnaryInstr* name(Value* value) { return unary_instr(op, value); }

#define BINARY_INSTR(name, op) \
  BinaryInstr* name(Value* lhs, Value* rhs) { return binary_instr(lhs, op, rhs); }

#define INT_COMPARE(name, pred) \
  IntCompare* name(Value* lhs, Value* rhs) { return int_compare(lhs, pred, rhs); }

#define CAST(name, kind) \
  Cast* name(Value* value, Type* target_type) { return cast(kind, value, target_type); }

  UNARY_INSTR(neg, UnaryOp::Neg)
  UNARY_INSTR(not_, UnaryOp::Not)

  BINARY_INSTR(add, BinaryOp::Add)
  BINARY_INSTR(sub, BinaryOp::Sub)
  BINARY_INSTR(mul, BinaryOp::Mul)
  BINARY_INSTR(umod, BinaryOp::ModU)
  BINARY_INSTR(udiv, BinaryOp::DivU)
  BINARY_INSTR(smod, BinaryOp::ModS)
  BINARY_INSTR(sdiv, BinaryOp::DivS)
  BINARY_INSTR(shr, BinaryOp::Shr)
  BINARY_INSTR(shl, BinaryOp::Shl)
  BINARY_INSTR(sar, BinaryOp::Sar)
  BINARY_INSTR(and_, BinaryOp::And)
  BINARY_INSTR(or_, BinaryOp::Or)
  BINARY_INSTR(xor_, BinaryOp::Xor)

  INT_COMPARE(compare_eq, IntPredicate::Equal)
  INT_COMPARE(compare_ne, IntPredicate::NotEqual)
  INT_COMPARE(compare_ugt, IntPredicate::GtU)
  INT_COMPARE(compare_ugte, IntPredicate::GteU)
  INT_COMPARE(compare_sgt, IntPredicate::GtS)
  INT_COMPARE(compare_sgte, IntPredicate::GteS)
  INT_COMPARE(compare_ult, IntPredicate::LtU)
  INT_COMPARE(compare_ulte, IntPredicate::LteU)
  INT_COMPARE(compare_slt, IntPredicate::LtS)
  INT_COMPARE(compare_slte, IntPredicate::LteS)

  CAST(zext, CastKind::ZeroExtend)
  CAST(sext, CastKind::SignExtend)
  CAST(trunc, CastKind::Truncate)
  CAST(bitcast, CastKind::Bitcast)

#undef UNARY_INSTR
#undef BINARY_INSTR
#undef INT_COMPARE
#undef CAST
};

}  // namespace flugzeug