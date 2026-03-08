#pragma once

#include "parser/token.h"
#include "sema/type.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace glsl2llvm::sema {

struct VariableSymbol {
  std::string name;
  TypeKind type = TypeKind::Unresolved;
  parser::SourceLocation location;
  bool is_builtin = false;
};

struct FunctionSymbol {
  std::string name;
  TypeKind return_type = TypeKind::Unresolved;
  std::vector<TypeKind> parameter_types;
  parser::SourceLocation location;
};

class SymbolTable {
 public:
  SymbolTable();

  void push_scope();
  void pop_scope();

  bool define_variable(const std::string &name, TypeKind type, parser::SourceLocation location,
                       bool is_builtin = false);
  const VariableSymbol *lookup_variable(const std::string &name) const;

  bool define_function(const std::string &name, TypeKind return_type,
                       const std::vector<TypeKind> &parameter_types,
                       parser::SourceLocation location);
  const FunctionSymbol *lookup_function(const std::string &name) const;

 private:
  std::vector<std::unordered_map<std::string, VariableSymbol>> variable_scopes_;
  std::unordered_map<std::string, FunctionSymbol> functions_;
};

}  // namespace glsl2llvm::sema
