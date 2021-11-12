#pragma once
#include "Instructions.hpp"

namespace flugzeug {

class Instruction;

class InstructionInserter {
  class Block* block = nullptr;
  class Context* context = nullptr;

  void insert_internal(Instruction* instruction);

  template <typename T> T* insert(T* instruction) {
    insert_internal(instruction);
    return instruction;
  }

public:
  explicit InstructionInserter(Block* block = nullptr) { set_insertion_block(block); }

  void set_insertion_block(Block* insertion_block) {
    if (insertion_block) {
      block = insertion_block;
      context = insertion_block->get_context();
    } else {
      block = nullptr;
      context = nullptr;
    }
  }

  UnaryInstr* unary_instr(UnaryOp op, Value* val);
  BinaryInstr* binary_instr(Value* lhs, BinaryOp op, Value* rhs);
  IntCompare* int_compare(Value* lhs, IntPredicate predicate, Value* rhs);
  Load* load(Value* ptr);
  Store* store(Value* ptr, Value* val);
  Call* call(Function* function, const std::vector<Value*>& arguments);
  Branch* branch(Block* target);
  CondBranch* cond_branch(Value* cond, Block* true_target, Block* false_target);
  StackAlloc* stack_alloc(Type type, size_t size = 1);
  Ret* ret(Value* val = nullptr);
  Offset* offset(Value* base, Value* index);
  Cast* cast(Value* val, CastKind kind, Type target_type);
  Select* select(Value* cond, Value* true_val, Value* false_val);
  Phi* phi(Type type);
  Phi* phi(const std::vector<Phi::Incoming>& incoming);

#define UNARY_INSTR(name, op)                                                                      \
  UnaryInstr* name(Value* val) { return unary_instr(op, val); }

#define BINARY_INSTR(name, op)                                                                     \
  BinaryInstr* name(Value* lhs, Value* rhs) { return binary_instr(lhs, op, rhs); }

#define INT_COMPARE(name, pred)                                                                    \
  IntCompare* name(Value* lhs, Value* rhs) { return int_compare(lhs, pred, rhs); }

#define CAST(name, kind)                                                                           \
  Cast* name(Value* val, Type target_type) { return cast(val, kind, target_type); }

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

} // namespace flugzeug