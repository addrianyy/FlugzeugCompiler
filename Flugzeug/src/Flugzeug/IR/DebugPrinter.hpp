#pragma once
#include "IRPrinter.hpp"

namespace flugzeug {

class DebugPrinter : public IRPrinter {
  void write_string(std::string_view string) override;

public:
  DebugPrinter() = default;
};

} // namespace flugzeug