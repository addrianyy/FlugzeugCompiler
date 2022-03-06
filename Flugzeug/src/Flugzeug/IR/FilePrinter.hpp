#pragma once
#include "IRPrinter.hpp"
#include <fstream>

namespace flugzeug {

class FilePrinter : public IRPrinter {
private:
  std::ofstream file_stream;

  void write_string(std::string_view string) override;

public:
  explicit FilePrinter(const std::string& path);
};

} // namespace flugzeug