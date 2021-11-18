#pragma once
#include "AST.hpp"
#include <cstdint>
#include <string_view>

namespace turboc {

class ASTPrinter {
  std::string indentation_str;

  void increase_indentation();
  void decrease_indentation();

  void print(std::string_view str);

  void key_internal(std::string_view name);

  template <typename T, typename... Args> void value_variadic(const T& v, Args... args) {
    value(v);
    value_variadic(args...);
  }
  void value_variadic() { print("\n"); }

public:
  void begin_structure(std::string_view name);
  void end_structure();

  void standalone_statement(const Stmt* stmt);
  void simple_structure(std::string_view str);

  void value(const Type& type);
  void value(UnaryOp op);
  void value(BinaryOp op);
  void value(std::string_view str);
  void value(const Stmt* stmt);
  void value(uint64_t num);

  template <typename Fn> void key(std::string_view name, Fn fn) {
    key_internal(name);
    fn();
    print("\n");
  }

  template <typename... Args> void key_value(std::string_view name, Args... args) {
    key_internal(name);
    value_variadic(args...);
  }
};

} // namespace turboc