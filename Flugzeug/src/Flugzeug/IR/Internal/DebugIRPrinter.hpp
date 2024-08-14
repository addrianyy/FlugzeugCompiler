#pragma once
#include <Flugzeug/IR/IRPrinter.hpp>

namespace flugzeug {

class DebugIRPrinter : public IRPrinter {
  void write_string(std::string_view string) override;

 public:
  DebugIRPrinter();
};

}  // namespace flugzeug