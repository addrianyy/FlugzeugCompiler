#pragma once
#include <fstream>
#include "IRPrinter.hpp"

namespace flugzeug {

class FileIRPrinter : public IRPrinter {
  std::ofstream file_stream;

  void write_string(std::string_view string) override;

 public:
  explicit FileIRPrinter(const std::string& path);
};

}  // namespace flugzeug