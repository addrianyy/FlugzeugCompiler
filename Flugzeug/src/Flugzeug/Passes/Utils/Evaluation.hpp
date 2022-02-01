#pragma once
#include <Flugzeug/IR/Instructions.hpp>

#include <cstdint>

namespace flugzeug::utils {

uint64_t evaluate_unary_instr(Type* type, UnaryOp op, uint64_t value);
uint64_t evaluate_binary_instr(Type* type, uint64_t lhs, BinaryOp op, uint64_t rhs);
bool evaluate_int_compare(Type* type, uint64_t lhs, IntPredicate predicate, uint64_t rhs);
uint64_t evaluate_cast(uint64_t from, Type* from_type, Type* to_type, CastKind cast_kind);

} // namespace flugzeug::utils