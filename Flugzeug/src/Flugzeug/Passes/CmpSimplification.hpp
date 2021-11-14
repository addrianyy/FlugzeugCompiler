#pragma once

namespace flugzeug {

class Function;
class IntCompare;

class CmpSimplification {
  static bool simplify_cmp(IntCompare* cmp);

public:
  static bool run(Function* function);
};

} // namespace flugzeug