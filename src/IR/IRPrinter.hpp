#pragma once
#include "Type.hpp"
#include <Core/ClassTraits.hpp>

#include <string_view>

namespace flugzeug {

class Value;
class Block;

class IRPrinter {
public:
  class LinePrinter {
    IRPrinter* ir_printer;

    bool comma_pending = false;
    bool space_pending = false;

  public:
    CLASS_NON_MOVABLE_NON_COPYABLE(LinePrinter)

    enum class SpecialItem {
      Comma,
      Colon,
      Equals,
      ParenOpen,
      ParenClose,
      BracketOpen,
      BracketClose,
    };

    struct NonKeywordWord {
      std::string_view text;
    };

    void begin_generic_item();
    void end_generic_item();

  private:
    void item(Type type);
    void item(const Value* value);
    void item(const Block* block);
    void item(std::string_view keyword);
    void item(size_t num);
    void item(SpecialItem special);
    void item(NonKeywordWord word);

  public:
    explicit LinePrinter(IRPrinter* printer) : ir_printer(printer) {}
    ~LinePrinter() { ir_printer->write_string("\n"); }

    void print() {}

    template <typename T, typename... Args> void print(const T& current, Args... args) {
      item(current);
      print(args...);
    }
  };

  friend class InstructionPrinter;

protected:
  virtual void begin_keyword() {}
  virtual void end_keyword() {}

  virtual void begin_value() {}
  virtual void end_value() {}

  virtual void begin_constant() {}
  virtual void end_constant() {}

  virtual void begin_type() {}
  virtual void end_type() {}

  virtual void begin_block() {}
  virtual void end_block() {}

  virtual void write_string(std::string_view string) = 0;

public:
  using Item = LinePrinter::SpecialItem;
  using NonKeywordWord = LinePrinter::NonKeywordWord;

  virtual ~IRPrinter() = default;

  LinePrinter create_line_printer() { return LinePrinter(this); }

  void tab() { write_string("  "); }
  void newline() { write_string("\n"); }
  void raw_write(std::string_view string) { write_string(string); }
};

} // namespace flugzeug