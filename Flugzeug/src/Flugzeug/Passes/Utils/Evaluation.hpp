#pragma once
#include <Flugzeug/IR/Instructions.hpp>

#include <cstdint>

namespace flugzeug::utils {

uint64_t evaluate_unary_instr(Type* type, UnaryOp op, uint64_t value);
uint64_t evaluate_binary_instr(Type* type, uint64_t lhs, BinaryOp op, uint64_t rhs);
bool evaluate_int_compare(Type* type, uint64_t lhs, IntPredicate predicate, uint64_t rhs);
uint64_t evaluate_cast(uint64_t from, Type* from_type, Type* to_type, CastKind cast_kind);

inline Constant* evaluate_unary_instr_to_value(Type* type, UnaryOp op, uint64_t value) {
  return type->constant(evaluate_unary_instr(type, op, value));
}

inline Constant* evaluate_binary_instr_to_value(Type* type,
                                                uint64_t lhs,
                                                BinaryOp op,
                                                uint64_t rhs) {
  return type->constant(evaluate_binary_instr(type, lhs, op, rhs));
}

inline Constant* evaluate_int_compare_to_value(Type* type,
                                               uint64_t lhs,
                                               IntPredicate predicate,
                                               uint64_t rhs) {
  return type->context()->i1_ty()->constant(evaluate_int_compare(type, lhs, predicate, rhs));
}

inline Constant* evaluate_cast_to_value(uint64_t from,
                                        Type* from_type,
                                        Type* to_type,
                                        CastKind cast_kind) {
  return to_type->constant(evaluate_cast(from, from_type, to_type, cast_kind));
}

}  // namespace flugzeug::utils