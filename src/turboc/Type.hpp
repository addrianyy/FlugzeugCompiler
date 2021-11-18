#pragma once
#include <cstdint>
#include <string>

namespace turboc {

class Type {
public:
  enum class Kind {
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    Void,
  };

private:
  Kind kind;
  uint32_t indirection;

public:
  explicit Type(Kind kind, uint32_t indirection = 0) : kind(kind), indirection(indirection) {}

  Kind get_kind() const { return kind; }
  uint32_t get_indirection() const { return indirection; }

  std::string format() const;
};

} // namespace turboc