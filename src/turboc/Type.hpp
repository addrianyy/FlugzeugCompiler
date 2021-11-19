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

  bool is_pointer() const { return indirection > 0; }
  bool is_arithmetic() const { return indirection == 0 && kind != Kind::Void; }
  bool is_nonvoid_pointer() const { return indirection > 0 && kind != Kind::Void; }
  bool is_void() const { return indirection == 0 && kind == Kind::Void; }

  Type strip_pointer() const;
  Type add_pointer() const;

  bool is_signed() const;
  size_t get_byte_size() const;

  std::string format() const;

  bool operator==(const Type& other) const {
    return kind == other.kind && indirection == other.indirection;
  }
};

} // namespace turboc