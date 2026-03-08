#include "sema/semantic_analyzer.h"

#include <memory>
#include <utility>

namespace glsl2llvm::sema {

namespace {

template <typename T>
T *as(ast::Node *node) {
  return dynamic_cast<T *>(node);
}

template <typename T>
const T *as(const ast::Node *node) {
  return dynamic_cast<const T *>(node);
}

}  // namespace

SemaResult SemanticAnalyzer::analyze(ast::TranslationUnit &unit) {
  errors_.clear();
  symbols_ = SymbolTable();

  unit.resolved_type = TypeKind::Void;

  // Builtin variable: simplified to vec3 for component access checks.
  symbols_.define_variable("gl_GlobalInvocationID", TypeKind::Vec3, parser::SourceLocation{}, true);

  // First pass: function signatures.
  for (auto &function_ptr : unit.functions) {
    ast::FunctionDecl &function = *function_ptr;

    TypeKind return_type = type_system_.resolve_type_name(function.return_type);
    if (return_type == TypeKind::Unresolved) {
      add_error(function.location,
                "unknown return type '" + function.return_type + "' for function '" + function.name + "'");
      return_type = TypeKind::Error;
    }
    function.resolved_type = return_type;

    std::vector<TypeKind> parameter_types;
    parameter_types.reserve(function.parameters.size());
    for (auto &param_ptr : function.parameters) {
      ast::VarDecl &param = *param_ptr;
      TypeKind param_type = type_system_.resolve_type_name(param.type_name);
      if (param_type == TypeKind::Unresolved) {
        add_error(param.location, "unknown parameter type '" + param.type_name + "'");
        param_type = TypeKind::Error;
      }
      if (param_type == TypeKind::Void) {
        add_error(param.location, "parameter '" + param.name + "' cannot have type void");
        param_type = TypeKind::Error;
      }
      param.resolved_type = param_type;
      parameter_types.push_back(param_type);
    }

    if (!symbols_.define_function(function.name, return_type, parameter_types, function.location)) {
      add_error(function.location, "duplicate function definition: '" + function.name + "'");
    }
  }

  for (auto &global_ptr : unit.global_vars) {
    analyze_global_var(*global_ptr);
  }

  for (auto &function_ptr : unit.functions) {
    analyze_function(*function_ptr);
  }

  return SemaResult{std::move(errors_)};
}

void SemanticAnalyzer::add_error(parser::SourceLocation location, std::string message) {
  errors_.push_back(SemaError{location, std::move(message)});
}

TypeKind SemanticAnalyzer::resolve_decl_type(ast::VarDecl &decl) {
  if (decl.is_buffer_block) {
    if (decl.buffer_element_type != "float") {
      add_error(decl.location, "only float buffer element type is supported");
      decl.resolved_type = TypeKind::Error;
      return TypeKind::Error;
    }
    decl.resolved_type = TypeKind::Buffer;
    return TypeKind::Buffer;
  }

  TypeKind type = type_system_.resolve_type_name(decl.type_name);

  if (type == TypeKind::Unresolved) {
    add_error(decl.location, "unknown type '" + decl.type_name + "' for variable '" + decl.name + "'");
    type = TypeKind::Error;
  }

  if (type == TypeKind::Void) {
    add_error(decl.location, "variable '" + decl.name + "' cannot have type void");
    type = TypeKind::Error;
  }

  decl.resolved_type = type;
  return type;
}

void SemanticAnalyzer::analyze_global_var(ast::VarDecl &decl) {
  const TypeKind declared_type = resolve_decl_type(decl);

  if (!symbols_.define_variable(decl.name, declared_type, decl.location)) {
    add_error(decl.location, "duplicate global variable definition: '" + decl.name + "'");
  }

  if (decl.initializer != nullptr) {
    const TypeKind init_type = analyze_expr(*decl.initializer);
    if (!type_system_.is_assignable(declared_type, init_type)) {
      add_error(decl.location,
                "type mismatch in global initializer for '" + decl.name + "': expected " +
                    std::string(to_string(declared_type)) + ", got " +
                    std::string(to_string(init_type)));
    }
  }
}

void SemanticAnalyzer::analyze_function(ast::FunctionDecl &function) {
  current_function_return_type_ = function.resolved_type;

  symbols_.push_scope();

  for (auto &param_ptr : function.parameters) {
    ast::VarDecl &param = *param_ptr;
    TypeKind type = type_system_.resolve_type_name(param.type_name);
    if (type == TypeKind::Unresolved || type == TypeKind::Void) {
      type = TypeKind::Error;
    }
    param.resolved_type = type;

    if (!symbols_.define_variable(param.name, type, param.location)) {
      add_error(param.location, "duplicate parameter definition: '" + param.name + "'");
    }
  }

  if (function.body != nullptr) {
    analyze_block(*function.body, false);
  }

  symbols_.pop_scope();
}

void SemanticAnalyzer::analyze_block(ast::BlockStmt &block, bool create_scope) {
  block.resolved_type = TypeKind::Void;

  if (create_scope) {
    symbols_.push_scope();
  }

  for (auto &statement : block.statements) {
    analyze_stmt(*statement);
  }

  if (create_scope) {
    symbols_.pop_scope();
  }
}

void SemanticAnalyzer::analyze_stmt(ast::Stmt &stmt) {
  switch (stmt.kind) {
    case ast::NodeKind::VarDecl: {
      auto &decl = *as<ast::VarDecl>(&stmt);
      const TypeKind declared_type = resolve_decl_type(decl);

      if (!symbols_.define_variable(decl.name, declared_type, decl.location)) {
        add_error(decl.location, "duplicate variable definition: '" + decl.name + "'");
      }

      if (decl.initializer != nullptr) {
        const TypeKind init_type = analyze_expr(*decl.initializer);
        if (!type_system_.is_assignable(declared_type, init_type)) {
          add_error(decl.location,
                    "type mismatch in initializer for '" + decl.name + "': expected " +
                        std::string(to_string(declared_type)) + ", got " +
                        std::string(to_string(init_type)));
        }
      }
      return;
    }
    case ast::NodeKind::BlockStmt: {
      auto &block = *as<ast::BlockStmt>(&stmt);
      analyze_block(block, true);
      return;
    }
    case ast::NodeKind::IfStmt: {
      auto &if_stmt = *as<ast::IfStmt>(&stmt);
      stmt.resolved_type = TypeKind::Void;

      if (if_stmt.condition != nullptr) {
        const TypeKind cond_type = analyze_expr(*if_stmt.condition);
        if (cond_type != TypeKind::Bool && cond_type != TypeKind::Error) {
          add_error(if_stmt.condition->location, "if condition must be bool");
        }
      }

      if (if_stmt.then_branch != nullptr) {
        analyze_stmt(*if_stmt.then_branch);
      }
      return;
    }
    case ast::NodeKind::ForStmt: {
      auto &for_stmt = *as<ast::ForStmt>(&stmt);
      stmt.resolved_type = TypeKind::Void;

      symbols_.push_scope();

      if (for_stmt.init_stmt != nullptr) {
        analyze_stmt(*for_stmt.init_stmt);
      }
      if (for_stmt.init_expr != nullptr) {
        analyze_expr(*for_stmt.init_expr);
      }
      if (for_stmt.condition != nullptr) {
        const TypeKind cond_type = analyze_expr(*for_stmt.condition);
        if (cond_type != TypeKind::Bool && cond_type != TypeKind::Error) {
          add_error(for_stmt.condition->location, "for condition must be bool");
        }
      }
      if (for_stmt.update != nullptr) {
        analyze_expr(*for_stmt.update);
      }
      if (for_stmt.body != nullptr) {
        analyze_stmt(*for_stmt.body);
      }

      symbols_.pop_scope();
      return;
    }
    case ast::NodeKind::ReturnStmt: {
      auto &ret = *as<ast::ReturnStmt>(&stmt);
      stmt.resolved_type = current_function_return_type_;

      if (ret.value == nullptr) {
        if (current_function_return_type_ != TypeKind::Void &&
            current_function_return_type_ != TypeKind::Error) {
          add_error(ret.location, "missing return value");
        }
        return;
      }

      const TypeKind value_type = analyze_expr(*ret.value);
      if (current_function_return_type_ == TypeKind::Void) {
        add_error(ret.location, "void function should not return a value");
        return;
      }

      if (!type_system_.is_assignable(current_function_return_type_, value_type)) {
        add_error(ret.location,
                  "return type mismatch: expected " +
                      std::string(to_string(current_function_return_type_)) + ", got " +
                      std::string(to_string(value_type)));
      }
      return;
    }
    case ast::NodeKind::ExprStmt: {
      auto &expr_stmt = *as<ast::ExprStmt>(&stmt);
      stmt.resolved_type = TypeKind::Void;
      if (expr_stmt.expression != nullptr) {
        (void)analyze_expr(*expr_stmt.expression);
      }
      return;
    }
    default:
      stmt.resolved_type = TypeKind::Error;
      add_error(stmt.location, "unsupported statement node during semantic analysis");
      return;
  }
}

TypeKind SemanticAnalyzer::analyze_expr(ast::Expr &expr) {
  switch (expr.kind) {
    case ast::NodeKind::LiteralExpr: {
      auto &literal = *as<ast::LiteralExpr>(&expr);
      if (literal.value_kind == ast::LiteralExpr::ValueKind::Number) {
        literal.resolved_type = type_system_.numeric_literal_type(literal.text);
        return literal.resolved_type;
      }

      const VariableSymbol *symbol = symbols_.lookup_variable(literal.text);
      if (symbol == nullptr) {
        add_error(literal.location, "undefined variable: '" + literal.text + "'");
        literal.resolved_type = TypeKind::Error;
        return literal.resolved_type;
      }

      literal.resolved_type = symbol->type;
      return literal.resolved_type;
    }
    case ast::NodeKind::BinaryExpr: {
      auto &binary = *as<ast::BinaryExpr>(&expr);

      TypeKind lhs_type = TypeKind::Error;
      TypeKind rhs_type = TypeKind::Error;
      if (binary.lhs != nullptr) {
        lhs_type = analyze_expr(*binary.lhs);
      }
      if (binary.rhs != nullptr) {
        rhs_type = analyze_expr(*binary.rhs);
      }

      const TypeKind result_type = type_system_.binary_result(binary.op, lhs_type, rhs_type);
      binary.resolved_type = result_type;

      if (result_type == TypeKind::Error && lhs_type != TypeKind::Error && rhs_type != TypeKind::Error) {
        add_error(binary.location,
                  "type mismatch in binary expression '" + binary.op + "': " +
                      std::string(to_string(lhs_type)) + " vs " + std::string(to_string(rhs_type)));
      }

      return binary.resolved_type;
    }
    case ast::NodeKind::CallExpr: {
      auto &call = *as<ast::CallExpr>(&expr);

      std::vector<TypeKind> arg_types;
      arg_types.reserve(call.args.size());
      for (auto &arg : call.args) {
        arg_types.push_back(analyze_expr(*arg));
      }

      const ast::LiteralExpr *callee_identifier =
          call.callee != nullptr ? as<ast::LiteralExpr>(call.callee.get()) : nullptr;
      if (callee_identifier == nullptr ||
          callee_identifier->value_kind != ast::LiteralExpr::ValueKind::Identifier) {
        if (call.callee != nullptr) {
          analyze_expr(*call.callee);
        }
        add_error(call.location, "only direct function calls are supported");
        call.resolved_type = TypeKind::Error;
        return call.resolved_type;
      }

      const FunctionSymbol *function = symbols_.lookup_function(callee_identifier->text);
      if (function == nullptr) {
        add_error(call.location, "undefined function: '" + callee_identifier->text + "'");
        call.resolved_type = TypeKind::Error;
        return call.resolved_type;
      }

      if (function->parameter_types.size() != arg_types.size()) {
        add_error(call.location,
                  "function argument count mismatch for '" + callee_identifier->text + "': expected " +
                      std::to_string(function->parameter_types.size()) + ", got " +
                      std::to_string(arg_types.size()));
      }

      const std::size_t check_count =
          function->parameter_types.size() < arg_types.size() ? function->parameter_types.size()
                                                              : arg_types.size();
      for (std::size_t i = 0; i < check_count; ++i) {
        if (!type_system_.is_assignable(function->parameter_types[i], arg_types[i])) {
          add_error(call.location,
                    "function argument type mismatch at index " + std::to_string(i) +
                        ": expected " + std::string(to_string(function->parameter_types[i])) +
                        ", got " + std::string(to_string(arg_types[i])));
        }
      }

      if (call.callee != nullptr) {
        call.callee->resolved_type = function->return_type;
      }
      call.resolved_type = function->return_type;
      return call.resolved_type;
    }
    case ast::NodeKind::MemberExpr: {
      auto &member = *as<ast::MemberExpr>(&expr);
      TypeKind object_type = TypeKind::Error;
      if (member.object != nullptr) {
        object_type = analyze_expr(*member.object);
      }

      if (const auto *obj_literal = member.object != nullptr ? as<ast::LiteralExpr>(member.object.get())
                                                              : nullptr;
          obj_literal != nullptr &&
          obj_literal->value_kind == ast::LiteralExpr::ValueKind::Identifier &&
          obj_literal->text == "gl_GlobalInvocationID") {
        if (member.member_name == "x" || member.member_name == "y" || member.member_name == "z") {
          member.resolved_type = TypeKind::Uint;
          return member.resolved_type;
        }
      }

      member.resolved_type = type_system_.member_access_type(object_type, member.member_name);
      if (member.resolved_type == TypeKind::Error && object_type != TypeKind::Error) {
        add_error(member.location,
                  "invalid member access '." + member.member_name + "' on type " +
                      std::string(to_string(object_type)));
      }

      return member.resolved_type;
    }
    case ast::NodeKind::IndexExpr: {
      auto &index = *as<ast::IndexExpr>(&expr);
      TypeKind object_type = TypeKind::Error;
      TypeKind index_type = TypeKind::Error;

      if (index.object != nullptr) {
        object_type = analyze_expr(*index.object);
      }
      if (index.index != nullptr) {
        index_type = analyze_expr(*index.index);
      }

      if (index_type != TypeKind::Int && index_type != TypeKind::Uint &&
          index_type != TypeKind::Error) {
        add_error(index.location, "index expression must be int/uint");
      }

      if (object_type == TypeKind::BufferData || object_type == TypeKind::Vec2 ||
          object_type == TypeKind::Vec3 || object_type == TypeKind::Vec4) {
        index.resolved_type = TypeKind::Float;
        return index.resolved_type;
      }

      if (object_type != TypeKind::Error) {
        add_error(index.location,
                  "type is not indexable: " + std::string(to_string(object_type)));
      }

      index.resolved_type = TypeKind::Error;
      return index.resolved_type;
    }
    default:
      expr.resolved_type = TypeKind::Error;
      add_error(expr.location, "unsupported expression node during semantic analysis");
      return expr.resolved_type;
  }
}

}  // namespace glsl2llvm::sema
