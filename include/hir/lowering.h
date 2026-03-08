#pragma once

#include "ast/ast.h"
#include "hir/hir.h"
#include "parser/token.h"
#include "sema/type_system.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace glsl2llvm::hir {

struct LoweringError {
  parser::SourceLocation location;
  std::string message;
};

struct LoweringResult {
  std::unique_ptr<Module> module;
  std::vector<LoweringError> errors;

  [[nodiscard]] bool ok() const { return errors.empty(); }
};

class ASTLowerer {
 public:
  LoweringResult lower(const ast::TranslationUnit &unit);

 private:
  struct FunctionSignature {
    sema::TypeKind return_type = sema::TypeKind::Unresolved;
    std::vector<sema::TypeKind> parameter_types;
  };

  void add_error(parser::SourceLocation location, std::string message);

  std::unique_ptr<Function> lower_function(const ast::FunctionDecl &function);
  std::unique_ptr<Block> lower_block(const ast::BlockStmt &block, bool create_scope);
  std::unique_ptr<Stmt> lower_stmt(const ast::Stmt &stmt);
  std::unique_ptr<Expr> lower_expr(const ast::Expr &expr);

  std::unique_ptr<Expr> cast_if_needed(std::unique_ptr<Expr> value, sema::TypeKind target_type);

  sema::TypeKind get_ast_type(const ast::Node &node) const;
  sema::TypeKind resolve_type_name(const std::string &name) const;
  static bool can_implicit_cast(sema::TypeKind from, sema::TypeKind to);

  void push_scope();
  void pop_scope();
  void define_variable(const std::string &name, sema::TypeKind type);
  sema::TypeKind lookup_variable(const std::string &name) const;

  std::vector<LoweringError> errors_;
  std::vector<std::unordered_map<std::string, sema::TypeKind>> variable_scopes_;
  std::unordered_map<std::string, sema::TypeKind> global_variables_;
  std::unordered_map<std::string, FunctionSignature> function_signatures_;
  sema::TypeSystem type_system_;
  sema::TypeKind current_return_type_ = sema::TypeKind::Void;
};

}  // namespace glsl2llvm::hir
