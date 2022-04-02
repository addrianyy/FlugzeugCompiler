#pragma once
#include <unordered_map>

namespace flugzeug {

class Function;
class Instruction;

class AllocatedRegisters {
  std::unordered_map<const Instruction*, uint32_t> registers;

public:
  explicit AllocatedRegisters(std::unordered_map<const Instruction*, uint32_t> registers)
      : registers(std::move(registers)) {}

  uint32_t get_register(const Instruction* instruction) const;
};

AllocatedRegisters allocate_registers(Function* function);

} // namespace flugzeug