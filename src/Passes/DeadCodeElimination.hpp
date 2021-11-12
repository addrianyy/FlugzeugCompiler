#pragma once
#include <vector>

namespace flugzeug {

class Function;
class Instruction;

class DeadCodeElimination {
  static bool try_to_eliminate(Instruction* instruction, std::vector<Instruction*>& worklist);

public:
  static bool run(Function* function);
};

} // namespace flugzeug