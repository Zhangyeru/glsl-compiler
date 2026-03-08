#pragma once

namespace glsl2llvm::sema {

enum class TypeKind {
  Unresolved,
  Error,
  Void,
  Bool,
  Int,
  Uint,
  Float,
  Vec2,
  Vec3,
  Vec4,
  Buffer,
  BufferData,
};

inline const char *to_string(TypeKind type) {
  switch (type) {
    case TypeKind::Unresolved:
      return "<unresolved>";
    case TypeKind::Error:
      return "<error>";
    case TypeKind::Void:
      return "void";
    case TypeKind::Bool:
      return "bool";
    case TypeKind::Int:
      return "int";
    case TypeKind::Uint:
      return "uint";
    case TypeKind::Float:
      return "float";
    case TypeKind::Vec2:
      return "vec2";
    case TypeKind::Vec3:
      return "vec3";
    case TypeKind::Vec4:
      return "vec4";
    case TypeKind::Buffer:
      return "buffer";
    case TypeKind::BufferData:
      return "buffer_data";
  }

  return "<unknown>";
}

inline bool is_vector_type(TypeKind type) {
  return type == TypeKind::Vec2 || type == TypeKind::Vec3 || type == TypeKind::Vec4;
}

inline bool is_scalar_type(TypeKind type) {
  return type == TypeKind::Bool || type == TypeKind::Int || type == TypeKind::Uint ||
         type == TypeKind::Float;
}

inline bool is_numeric_type(TypeKind type) {
  return type == TypeKind::Int || type == TypeKind::Uint || type == TypeKind::Float ||
         is_vector_type(type);
}

}  // namespace glsl2llvm::sema
