#include "Context.hpp"
#include "Function.hpp"
#include "Module.hpp"

#include <Flugzeug/Core/HashCombine.hpp>

using namespace flugzeug;

size_t Context::ConstantKeyHash::operator()(const Context::ConstantKey& p) const {
  return combine_hash(p.constant, p.type);
}

size_t Context::PointerKeyHash::operator()(const Context::PointerKey& p) const {
  return combine_hash(p.indirection, p.base);
}

void Context::increase_refcount() {
  refcount++;
}
void Context::decrease_refcount() {
  verify(--refcount >= 0, "Refcount became negative");
}

Context::Context() {
  i1_type = new I1Type(this);
  i8_type = new I8Type(this);
  i16_type = new I16Type(this);
  i32_type = new I32Type(this);
  i64_type = new I64Type(this);
  void_type = new VoidType(this);
  block_type = new BlockType(this);
  function_type = new FunctionType(this);
}

Context::~Context() {
  for (auto [key, constant] : constants) {
    delete constant;
  }

  for (auto [key, undef] : undefs) {
    delete undef;
  }

  constexpr size_t base_type_count = 8;
  verify(refcount == base_type_count + pointer_types.size(), "Unexpected context refcount");

  for (auto [key, type] : pointer_types) {
    delete type;
  }

  delete i1_type;
  delete i8_type;
  delete i16_type;
  delete i32_type;
  delete i64_type;
  delete void_type;
  delete block_type;
  delete function_type;

  verify(refcount == 0, "Context refcount is not zero");
}

Constant* Context::get_constant(Type* type, uint64_t constant) {
  verify(!type->is_void() && !type->is_block() && !type->is_function(),
         "Cannot create constant with that type.");

  constant = Constant::constrain_u(type, constant);

  const ConstantKey key{type, constant};

  const auto it = constants.find(key);
  if (it != constants.end()) {
    return it->second;
  }

  auto result = new Constant(this, type, constant);
  constants[key] = result;
  return result;
}

Undef* Context::get_undef(Type* type) {
  verify(!type->is_void() && !type->is_block() && !type->is_function(),
         "Cannot create undef with that type.");

  const auto it = undefs.find(type);
  if (it != undefs.end()) {
    return it->second;
  }

  auto result = new Undef(this, type);
  undefs[type] = result;
  return result;
}

PointerType* Context::get_pointer_type_internal(Type* base, uint32_t indirection) {
  PointerKey key{base, indirection};

  {
    const auto it = pointer_types.find(key);
    if (it != pointer_types.end()) {
      return it->second;
    }
  }

  verify(indirection > 0, "Cannot create pointer with no indirection");

  switch (base->get_kind()) {
    case Type::Kind::Void:
    case Type::Kind::Block:
    case Type::Kind::Function:
    case Type::Kind::I1:
    case Type::Kind::Pointer:
      fatal_error("Invalid pointer base.");

    default:
      break;
  }

  PointerType* type = nullptr;

  if (indirection == 1) {
    type = new PointerType(this, base, base, indirection);
  } else {
    auto pointee = get_pointer_type_internal(base, indirection - 1);
    type = new PointerType(this, base, pointee, indirection);
  }

  pointer_types[key] = type;
  return type;
}

PointerType* Context::get_pointer_type(Type* pointee, uint32_t indirection) {
  Type* base = pointee;
  if (const auto pointer = cast<PointerType>(pointee)) {
    base = pointer->get_base();
    indirection += pointer->get_indirection();
  }

  return get_pointer_type_internal(base, indirection);
}

Module* Context::create_module() {
  return new Module(this);
}

#include <Flugzeug/Core/Files.hpp>
#include "Parser/ModuleParser.hpp"

Module* Context::create_module_from_source(std::string source) {
  const auto module = create_module();

  parse_to_module(std::move(source), module);

  return module;
}

Module* Context::create_module_from_file(const std::string& path) {
  return create_module_from_source(File::read_to_string(path));
}