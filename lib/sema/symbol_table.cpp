#include "sema/symbol_table.h"

namespace glsl2llvm::sema {

SymbolTable::SymbolTable() {
  push_scope();
}

void SymbolTable::push_scope() {
  variable_scopes_.emplace_back();
}

void SymbolTable::pop_scope() {
  if (!variable_scopes_.empty()) {
    variable_scopes_.pop_back();
  }
}

bool SymbolTable::define_variable(const std::string &name, TypeKind type,
                                  parser::SourceLocation location, bool is_builtin) {
  if (variable_scopes_.empty()) {
    push_scope();
  }

  auto &scope = variable_scopes_.back();
  if (scope.find(name) != scope.end()) {
    return false;
  }

  scope.emplace(name, VariableSymbol{name, type, location, is_builtin});
  return true;
}

const VariableSymbol *SymbolTable::lookup_variable(const std::string &name) const {
  for (auto it = variable_scopes_.rbegin(); it != variable_scopes_.rend(); ++it) {
    const auto found = it->find(name);
    if (found != it->end()) {
      return &found->second;
    }
  }

  return nullptr;
}

bool SymbolTable::define_function(const std::string &name, TypeKind return_type,
                                  const std::vector<TypeKind> &parameter_types,
                                  parser::SourceLocation location) {
  if (functions_.find(name) != functions_.end()) {
    return false;
  }

  functions_.emplace(name, FunctionSymbol{name, return_type, parameter_types, location});
  return true;
}

const FunctionSymbol *SymbolTable::lookup_function(const std::string &name) const {
  const auto found = functions_.find(name);
  if (found == functions_.end()) {
    return nullptr;
  }

  return &found->second;
}

}  // namespace glsl2llvm::sema
