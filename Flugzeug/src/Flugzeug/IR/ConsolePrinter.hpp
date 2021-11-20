#pragma once
#include "IRPrinter.hpp"

namespace flugzeug {

class ConsolePrinter : public IRPrinter {
public:
  enum class Variant {
    Simple,
    Colorful,
    ColorfulIfSupported,
  };

  std::ostream& output_stream;

private:
  Variant variant;

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

public:
  explicit ConsolePrinter(Variant variant);
  ConsolePrinter(Variant variant, std::ostream& output_stream);
  ~ConsolePrinter() override;

  void write_string(std::string_view string) override;
};

} // namespace flugzeug