#pragma once

namespace flugzeug {

class Function;

namespace opt {

enum class DeduplicationStrategy {
  BlockLocal,
  Global,
};

class InstructionDeduplication {
public:
  static bool run(Function* function, DeduplicationStrategy strategy);
};

} // namespace opt

} // namespace flugzeug