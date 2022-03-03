#include "InstructionInserter.hpp"

using namespace flugzeug;

void InstructionInserter::insert_internal(Instruction* instruction) {
  switch (insert_type) {

  case InsertType::BlockFront:
    insertion_block->push_instruction_front(instruction);
    break;

  case InsertType::BlockBack:
    insertion_block->push_instruction_back(instruction);
    break;

  case InsertType::BeforeInstruction:
    instruction->insert_before(insertion_instruction);
    if (follow_instruction) {
      insertion_instruction = instruction;
    }
    break;

  case InsertType::AfterInstruction:
    instruction->insert_after(insertion_instruction);
    if (follow_instruction) {
      insertion_instruction = instruction;
    }
    break;
  }
}

void InstructionInserter::set_insertion_block(Block* block, InsertDestination destination) {
  insert_type =
    destination == InsertDestination::Back ? InsertType::BlockBack : InsertType::BlockFront;
  follow_instruction = false;
  insertion_instruction = nullptr;
  insertion_block = block;
  context = block ? block->get_context() : nullptr;
}

void InstructionInserter::set_insertion_instruction(Instruction* instruction,
                                                    InsertDestination destination,
                                                    bool follow_instruction_) {
  insert_type = destination == InsertDestination::Back ? InsertType::AfterInstruction
                                                       : InsertType::BeforeInstruction;
  follow_instruction = follow_instruction_;
  insertion_instruction = instruction;
  insertion_block = nullptr;
  context = instruction ? instruction->get_context() : nullptr;
}

Block* InstructionInserter::get_insertion_block() {
  if (insertion_block) {
    return insertion_block;
  }

  if (insertion_instruction) {
    return insertion_instruction->get_block();
  }

  return nullptr;
}

UnaryInstr* InstructionInserter::unary_instr(UnaryOp op, Value* val) {
  return insert(new UnaryInstr(context, op, val));
}

BinaryInstr* InstructionInserter::binary_instr(Value* lhs, BinaryOp op, Value* rhs) {
  return insert(new BinaryInstr(context, lhs, op, rhs));
}

IntCompare* InstructionInserter::int_compare(Value* lhs, IntPredicate predicate, Value* rhs) {
  return insert(new IntCompare(context, lhs, predicate, rhs));
}

Load* InstructionInserter::load(Value* ptr) { return insert(new Load(context, ptr)); }

Store* InstructionInserter::store(Value* ptr, Value* val) {
  return insert(new Store(context, ptr, val));
}

Call* InstructionInserter::call(Function* function, const std::vector<Value*>& arguments) {
  return insert(new Call(context, function, arguments));
}

Branch* InstructionInserter::branch(Block* target) { return insert(new Branch(context, target)); }

CondBranch* InstructionInserter::cond_branch(Value* cond, Block* true_target, Block* false_target) {
  return insert(new CondBranch(context, cond, true_target, false_target));
}

StackAlloc* InstructionInserter::stack_alloc(Type* type, size_t size) {
  return insert(new StackAlloc(context, type, size));
}

Ret* InstructionInserter::ret(Value* val) { return insert(new Ret(context, val)); }

Offset* InstructionInserter::offset(Value* base, Value* index) {
  return insert(new Offset(context, base, index));
}

Cast* InstructionInserter::cast(Value* val, CastKind kind, Type* target_type) {
  return insert(new Cast(context, val, kind, target_type));
}

Select* InstructionInserter::select(Value* cond, Value* true_val, Value* false_val) {
  return insert(new Select(context, cond, true_val, false_val));
}

Phi* InstructionInserter::phi(Type* type) { return insert(new Phi(context, type)); }

Phi* InstructionInserter::phi(const std::vector<Phi::Incoming>& incoming) {
  return insert(new Phi(context, incoming));
}