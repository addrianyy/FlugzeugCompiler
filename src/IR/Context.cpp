#include "Context.hpp"
#include "Function.hpp"

using namespace flugzeug;

size_t Context::ConstantKeyHash::operator()(const Context::ConstantKey& p) const {
  const auto h1 = std::hash<uint64_t>{}(p.constant);
  const auto h2 = std::hash<uint32_t>{}(p.type.get_indirection());
  const auto h3 = std::hash<Type::Kind>{}(p.type.get_kind());

  size_t seed = 0ull;

  seed ^= h1 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);

  return seed;
}

size_t Context::TypeHash::operator()(const Type& p) const {
  const auto h1 = std::hash<uint32_t>{}(p.get_indirection());
  const auto h2 = std::hash<Type::Kind>{}(p.get_kind());

  size_t seed = 0ull;

  seed ^= h1 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);

  return seed;
}

void Context::increase_refcount() { refcount++; }
void Context::decrease_refcount() { verify(--refcount >= 0, "Refcount became negative"); }

Context::~Context() {
  for (auto [key, constant] : constants) {
    delete constant;
  }

  for (auto [key, undef] : undefs) {
    delete undef;
  }

  verify(refcount == 0, "Context refcount is not zero");
}

Constant* Context::get_constant(Type type, uint64_t constant) {
  verify(type.get_kind() != Type::Kind::Void && type.get_kind() != Type::Kind::Block,
         "Cannot create constant with that type.");

  Constant::constrain_constant(type, constant, &constant, nullptr);

  const ConstantKey key{type, constant};

  const auto it = constants.find(key);
  if (it != constants.end()) {
    return it->second;
  }

  auto result = new Constant(this, type, constant);

  constants[key] = result;

  return result;
}

Undef* Context::get_undef(Type type) {
  verify(type.get_kind() != Type::Kind::Void && type.get_kind() != Type::Kind::Block,
         "Cannot create undef with that type.");

  const auto it = undefs.find(type);
  if (it != undefs.end()) {
    return it->second;
  }

  auto result = new Undef(this, type);

  undefs[type] = result;

  return result;
}

Function* Context::create_function(Type return_type, std::string name,
                                   const std::vector<Type>& arguments) {
  return new Function(this, return_type, std::move(name), arguments);
}
