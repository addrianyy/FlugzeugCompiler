#pragma once
#include "Instructions.hpp"

namespace flugzeug {

namespace detail {

template <bool Constant>
class InstructionVisitor {
 protected:
  template <typename T>
  using Argument = std::conditional_t<Constant, const T, T>*;

 public:
  // Copy to implementation:
  /*
  void visit_unary_instr(Argument<UnaryInstr> unary) {}
  void visit_binary_instr(Argument<BinaryInstr> binary) {}
  void visit_int_compare(Argument<IntCompare> int_compare) {}
  void visit_load(Argument<Load> load) {}
  void visit_store(Argument<Store> store) {}
  void visit_call(Argument<Call> call) {}
  void visit_branch(Argument<Branch> branch) {}
  void visit_cond_branch(Argument<CondBranch> cond_branch) {}
  void visit_stackalloc(Argument<StackAlloc> stackalloc) {}
  void visit_ret(Argument<Ret> ret) {}
  void visit_offset(Argument<Offset> offset) {}
  void visit_cast(Argument<Cast> cast) {}
  void visit_select(Argument<Select> select) {}
  void visit_phi(Argument<Phi> phi) {}
   */
};

}  // namespace detail

using InstructionVisitor = detail::InstructionVisitor<false>;
using ConstInstructionVisitor = detail::InstructionVisitor<true>;

namespace visitor {
template <typename TInstruction, typename TVisitor>
inline auto visit_instruction(TInstruction* instruction, TVisitor& visitor) {
  using VisitorType = std::remove_cvref_t<TVisitor>;
  static_assert(std::is_base_of_v<InstructionVisitor, VisitorType> ||
                  std::is_base_of_v<ConstInstructionVisitor, VisitorType>,
                "Cannot visit using visitor that is not derived from InstructionVisitor");
  static_assert(std::is_base_of_v<Instruction, std::remove_cvref_t<TInstruction>>,
                "Cannot visit non-instruction argument");

  switch (instruction->get_kind()) {
    case Value::Kind::UnaryInstr:
      return visitor.visit_unary_instr(relaxed_cast<UnaryInstr>(instruction));
    case Value::Kind::BinaryInstr:
      return visitor.visit_binary_instr(relaxed_cast<BinaryInstr>(instruction));
    case Value::Kind::IntCompare:
      return visitor.visit_int_compare(relaxed_cast<IntCompare>(instruction));
    case Value::Kind::Load:
      return visitor.visit_load(relaxed_cast<Load>(instruction));
    case Value::Kind::Store:
      return visitor.visit_store(relaxed_cast<Store>(instruction));
    case Value::Kind::Call:
      return visitor.visit_call(relaxed_cast<Call>(instruction));
    case Value::Kind::Branch:
      return visitor.visit_branch(relaxed_cast<Branch>(instruction));
    case Value::Kind::CondBranch:
      return visitor.visit_cond_branch(relaxed_cast<CondBranch>(instruction));
    case Value::Kind::StackAlloc:
      return visitor.visit_stackalloc(relaxed_cast<StackAlloc>(instruction));
    case Value::Kind::Ret:
      return visitor.visit_ret(relaxed_cast<Ret>(instruction));
    case Value::Kind::Offset:
      return visitor.visit_offset(relaxed_cast<Offset>(instruction));
    case Value::Kind::Cast:
      return visitor.visit_cast(relaxed_cast<Cast>(instruction));
    case Value::Kind::Select:
      return visitor.visit_select(relaxed_cast<Select>(instruction));
    case Value::Kind::Phi:
      return visitor.visit_phi(relaxed_cast<Phi>(instruction));
    default:
      unreachable();
  }
}
}  // namespace visitor

}  // namespace flugzeug
