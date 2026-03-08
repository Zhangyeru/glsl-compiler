#include "sema/type_system.h"

#include <string>

namespace glsl2llvm::sema {

TypeKind TypeSystem::resolve_type_name(std::string_view name) const {
  if (name == "void") {
    return TypeKind::Void;
  }
  if (name == "bool") {
    return TypeKind::Bool;
  }
  if (name == "int") {
    return TypeKind::Int;
  }
  if (name == "uint") {
    return TypeKind::Uint;
  }
  if (name == "float") {
    return TypeKind::Float;
  }
  if (name == "vec2") {
    return TypeKind::Vec2;
  }
  if (name == "vec3") {
    return TypeKind::Vec3;
  }
  if (name == "vec4") {
    return TypeKind::Vec4;
  }
  if (name == "__buffer") {
    return TypeKind::Buffer;
  }

  return TypeKind::Unresolved;
}

TypeKind TypeSystem::numeric_literal_type(std::string_view literal) const {
  return literal.find('.') == std::string_view::npos ? TypeKind::Int : TypeKind::Float;
}

bool TypeSystem::is_assignable(TypeKind destination, TypeKind source) const {
  if (destination == TypeKind::Error || source == TypeKind::Error) {
    return true;
  }
  if (destination == TypeKind::Buffer || destination == TypeKind::BufferData ||
      source == TypeKind::Buffer || source == TypeKind::BufferData) {
    return destination == source;
  }
  return destination == source;
}

TypeKind TypeSystem::binary_result(std::string_view op, TypeKind lhs, TypeKind rhs) const {
  if (lhs == TypeKind::Error || rhs == TypeKind::Error) {
    return TypeKind::Error;
  }

  if (op == "=") {
    return is_assignable(lhs, rhs) ? lhs : TypeKind::Error;
  }

  if (op == "+" || op == "-" || op == "*" || op == "/") {
    if (lhs == rhs && is_numeric_type(lhs)) {
      return lhs;
    }
    return TypeKind::Error;
  }

  return TypeKind::Error;
}

TypeKind TypeSystem::member_access_type(TypeKind object_type, std::string_view member_name) const {
  if (member_name.size() != 1) {
    if (object_type == TypeKind::Buffer && member_name == "data") {
      return TypeKind::BufferData;
    }
    return TypeKind::Error;
  }

  const char c = member_name.front();
  if (object_type == TypeKind::Vec2 && (c == 'x' || c == 'y')) {
    return TypeKind::Float;
  }
  if (object_type == TypeKind::Vec3 && (c == 'x' || c == 'y' || c == 'z')) {
    return TypeKind::Float;
  }
  if (object_type == TypeKind::Vec4 && (c == 'x' || c == 'y' || c == 'z' || c == 'w')) {
    return TypeKind::Float;
  }

  return TypeKind::Error;
}

}  // namespace glsl2llvm::sema
