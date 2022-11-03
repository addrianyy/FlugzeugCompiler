#pragma once
#include <string>
#include <vector>

namespace flugzeug {

class Block;
class Instruction;

enum class ValidationBehaviour {
  Silent,
  ErrorsToStdout,
  ErrorsAreFatal,
};

class ValidationResults {
 public:
  struct Error {
    const char* source_file;
    int source_line;

    const Block* block;
    const Instruction* instruction;

    std::string description;
  };

 private:
  std::vector<Error> errors;

 public:
  explicit ValidationResults(std::vector<Error> errors) : errors(std::move(errors)) {}

  using iterator = std::vector<Error>::const_iterator;

  iterator begin() const { return errors.begin(); }
  iterator end() const { return errors.end(); }

  bool has_errors() const { return !errors.empty(); }
  const std::vector<Error>& get_errors() const { return errors; }
};

}  // namespace flugzeug