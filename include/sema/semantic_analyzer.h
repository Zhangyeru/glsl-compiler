#pragma once

#include "ast/ast.h"
#include "sema/symbol_table.h"
#include "sema/type_system.h"

#include <string>
#include <vector>

namespace glsl2llvm::sema {

struct SemaError {
  parser::SourceLocation location;
  std::string message;
};

struct SemaResult {
  std::vector<SemaError> errors;

  [[nodiscard]] bool ok() const { return errors.empty(); }
};

class SemanticAnalyzer {
 public:
  SemaResult analyze(ast::TranslationUnit &unit);

 private:
  void add_error(parser::SourceLocation location, std::string message);

  void analyze_global_var(ast::VarDecl &decl);
  void analyze_function(ast::FunctionDecl &function);
  void analyze_stmt(ast::Stmt &stmt);
  void analyze_block(ast::BlockStmt &block, bool create_scope);
  TypeKind analyze_expr(ast::Expr &expr);

  TypeKind resolve_decl_type(ast::VarDecl &decl);

  TypeSystem type_system_;
  SymbolTable symbols_;
  std::vector<SemaError> errors_;
  TypeKind current_function_return_type_ = TypeKind::Void;
};

}  // namespace glsl2llvm::sema
