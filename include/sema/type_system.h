#pragma once

#include "sema/type.h"

#include <string_view>

namespace glsl2llvm::sema {

class TypeSystem {
 public:
  TypeKind resolve_type_name(std::string_view name) const;
  TypeKind numeric_literal_type(std::string_view literal) const;

  bool is_assignable(TypeKind destination, TypeKind source) const;
  TypeKind binary_result(std::string_view op, TypeKind lhs, TypeKind rhs) const;

  TypeKind member_access_type(TypeKind object_type, std::string_view member_name) const;
};

}  // namespace glsl2llvm::sema
