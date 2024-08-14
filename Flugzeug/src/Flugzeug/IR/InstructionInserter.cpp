#include "InstructionInserter.hpp"

using namespace flugzeug;

void InstructionInserter::insert_internal(Instruction* instruction) {
  switch (insert_type) {
    case InsertType::BlockFront:
      insertion_block_->push_instruction_front(instruction);
      break;

    case InsertType::BlockBack:
      insertion_block_->push_instruction_back(instruction);
      break;

    case InsertType::BeforeInstruction:
      instruction->insert_before(insertion_instruction_);
      if (follow_instruction_) {
        insertion_instruction_ = instruction;
      }
      break;

    case InsertType::AfterInstruction:
      instruction->insert_after(insertion_instruction_);
      if (follow_instruction_) {
        insertion_instruction_ = instruction;
      }
      break;
  }
}

void InstructionInserter::set_insertion_block(Block* block, InsertDestination destination) {
  insert_type =
    destination == InsertDestination::Back ? InsertType::BlockBack : InsertType::BlockFront;
  follow_instruction_ = false;
  insertion_instruction_ = nullptr;
  insertion_block_ = block;
  context = block ? block->context() : nullptr;
}

void InstructionInserter::set_insertion_instruction(Instruction* instruction,
                                                    InsertDestination destination,
                                                    bool follow_instruction) {
  insert_type = destination == InsertDestination::Back ? InsertType::AfterInstruction
                                                       : InsertType::BeforeInstruction;
  follow_instruction_ = follow_instruction;
  insertion_instruction_ = instruction;
  insertion_block_ = nullptr;
  context = instruction ? instruction->context() : nullptr;
}

Block* InstructionInserter::insertion_block() {
  if (insertion_block_) {
    return insertion_block_;
  }

  if (insertion_instruction_) {
    return insertion_instruction_->block();
  }

  return nullptr;
}

UnaryInstr* InstructionInserter::unary_instr(UnaryOp op, Value* value) {
  return insert(new UnaryInstr(context, op, value));
}

BinaryInstr* InstructionInserter::binary_instr(Value* lhs, BinaryOp op, Value* rhs) {
  return insert(new BinaryInstr(context, lhs, op, rhs));
}

IntCompare* InstructionInserter::int_compare(Value* lhs, IntPredicate predicate, Value* rhs) {
  return insert(new IntCompare(context, lhs, predicate, rhs));
}

Load* InstructionInserter::load(Value* address) {
  return insert(new Load(context, address));
}

Store* InstructionInserter::store(Value* address, Value* stored_value) {
  return insert(new Store(context, address, stored_value));
}

Call* InstructionInserter::call(Function* function, const std::vector<Value*>& arguments) {
  return insert(new Call(context, function, arguments));
}

Branch* InstructionInserter::branch(Block* target) {
  return insert(new Branch(context, target));
}

CondBranch* InstructionInserter::cond_branch(Value* condition,
                                             Block* true_target,
                                             Block* false_target) {
  return insert(new CondBranch(context, condition, true_target, false_target));
}

StackAlloc* InstructionInserter::stack_alloc(Type* type, size_t size) {
  return insert(new StackAlloc(context, type, size));
}

Ret* InstructionInserter::ret(Value* value) {
  return insert(new Ret(context, value));
}

Offset* InstructionInserter::offset(Value* base, Value* index) {
  return insert(new Offset(context, base, index));
}

Cast* InstructionInserter::cast(CastKind kind, Value* casted_value, Type* target_type) {
  return insert(new Cast(context, kind, casted_value, target_type));
}

Select* InstructionInserter::select(Value* condition, Value* true_value, Value* false_value) {
  return insert(new Select(context, condition, true_value, false_value));
}

Phi* InstructionInserter::phi(Type* type) {
  return insert(new Phi(context, type));
}

Phi* InstructionInserter::phi(const std::vector<Phi::Incoming>& incoming) {
  return insert(new Phi(context, incoming));
}