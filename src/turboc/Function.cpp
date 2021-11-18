#include "Function.hpp"

#include <iostream>

using namespace turboc;

void FunctionPrototype::print(ASTPrinter& printer) const {
  printer.begin_structure("FunctionPrototype");

  printer.key_value("return type", return_type);
  printer.key_value("name", name);

  for (size_t i = 0; i < arguments.size(); ++i) {
    std::string key = "argument " + std::to_string(i);
    printer.key_value(key, arguments[i].first, " ", arguments[i].second);
  }

  printer.end_structure();
}

void Function::print(ASTPrinter& printer) const {
  printer.begin_structure("Function");

  printer.key("prototype", [&]() { prototype.print(printer); });
  printer.key("body", [&]() {
    if (body) {
      body->print(printer);
    } else {
      printer.value("none (extern function)");
    }
  });

  printer.end_structure();

  std::cout << '\n';
}