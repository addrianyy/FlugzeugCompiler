#pragma once
#include "AST.hpp"
#include "ASTPrinter.hpp"
#include "Type.hpp"

#include <memory>
#include <string>
#include <vector>

namespace turboc {

class BodyStmt;

class FunctionPrototype {
public:
  using Argument = std::pair<Type, std::string>;

private:
  std::string name;
  std::vector<Argument> arguments;
  Type return_type;

public:
  FunctionPrototype(std::string name, std::vector<Argument> arguments, const Type& return_type)
      : name(std::move(name)), arguments(std::move(arguments)), return_type(return_type) {}

  const std::string& get_name() { return name; }
  const std::vector<Argument>& get_arguments() { return arguments; }
  Type get_return_type() { return return_type; }

  void print(ASTPrinter& printer) const;
};

class Function {
  FunctionPrototype prototype;
  std::unique_ptr<BodyStmt> body;

public:
  Function(FunctionPrototype prototype, std::unique_ptr<BodyStmt> body)
      : prototype(std::move(prototype)), body(std::move(body)) {}

  const FunctionPrototype& get_prototype() const { return prototype; }
  const std::unique_ptr<BodyStmt>& get_body() const { return body; }

  void print(ASTPrinter& printer) const;
};

} // namespace turboc