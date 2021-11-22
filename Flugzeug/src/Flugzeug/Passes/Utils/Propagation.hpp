#pragma once
#include <Flugzeug/IR/Instructions.hpp>

#include <cstdint>

namespace flugzeug::utils {

uint64_t propagate_unary_instr(Type* type, UnaryOp op, uint64_t value);
uint64_t propagate_binary_instr(Type* type, uint64_t lhs, BinaryOp op, uint64_t rhs);
bool propagate_int_compare(Type* type, uint64_t lhs, IntPredicate predicate, uint64_t rhs);

} // namespace flugzeug::utils