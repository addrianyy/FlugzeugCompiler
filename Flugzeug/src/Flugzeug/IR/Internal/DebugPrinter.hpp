#pragma once
#include <Flugzeug/IR/IRPrinter.hpp>

namespace flugzeug {

class DebugPrinter : public IRPrinter {
  void write_string(std::string_view string) override;

 public:
  DebugPrinter();
};

}  // namespace flugzeug