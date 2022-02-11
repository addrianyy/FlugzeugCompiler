#pragma once

namespace flugzeug {

class Function;

enum class DeduplicationStrategy {
  BlockLocal,
  Global,
};

class InstructionDeduplication {
public:
  static bool run(Function* function, DeduplicationStrategy strategy);
};

} // namespace flugzeug