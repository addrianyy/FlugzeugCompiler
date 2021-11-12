#pragma once
#include <cstdint>
#include <string>

namespace flugzeug {

class Type {
public:
  enum class Kind {
    Void = 0,
    I1 = 1,
    I8 = 8,
    I16 = 16,
    I32 = 32,
    I64 = 64,
    Block = 1024,
  };

private:
  Kind kind;
  uint32_t indirection;

public:
  Type(Kind kind, uint32_t indirection = 0) : kind(kind), indirection(indirection) {}

  bool is_pointer() const { return indirection > 0; }

  Kind get_kind() const { return kind; }
  uint32_t get_indirection() const { return indirection; }

  Type deref() const;
  Type ref() const;

  size_t get_bit_size() const;
  uint64_t get_bit_mask() const;

  bool operator==(const Type& other) const {
    return kind == other.kind && indirection == other.indirection;
  }
  bool operator==(const Kind other) const { return kind == other && indirection == 0; }

  bool is_void() const { return kind == Kind::Void; }

  std::string format() const;
};

} // namespace flugzeug