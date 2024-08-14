#pragma once
#include "IRPrinter.hpp"

namespace flugzeug {

class ConsoleIRPrinter : public IRPrinter {
 public:
  enum class Variant {
    Simple,
    Colorful,
    ColorfulIfSupported,
  };

 private:
  Variant variant;
  std::ostream& output_stream;

  void set_color(int color);
  void reset();

  void begin_keyword() override;
  void begin_value() override;
  void begin_constant() override;
  void begin_type() override;
  void begin_block() override;

  void end_keyword() override { reset(); }
  void end_value() override { reset(); }
  void end_constant() override { reset(); }
  void end_type() override { reset(); }
  void end_block() override { reset(); }

  void write_string(std::string_view string) override;

 public:
  explicit ConsoleIRPrinter(Variant variant);
  ConsoleIRPrinter(Variant variant, std::ostream& output_stream);
  ~ConsoleIRPrinter() override;
};

}  // namespace flugzeug