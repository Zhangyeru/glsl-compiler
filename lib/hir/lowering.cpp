#include "hir/lowering.h"

#include <memory>
#include <utility>

namespace glsl2llvm::hir {

namespace {

template <typename T>
const T *as(const ast::Node *node) {
  return dynamic_cast<const T *>(node);
}

bool is_vector_constructor_name(const std::string &name) {
  return name == "vec2" || name == "vec3" || name == "vec4";
}

std::unique_ptr<Block> stmt_to_block(std::unique_ptr<Stmt> stmt) {
  auto block = std::make_unique<Block>();
  if (stmt == nullptr) {
    return block;
  }

  if (stmt->kind == NodeKind::BlockStmt) {
    auto wrapped = std::unique_ptr<BlockStmt>(static_cast<BlockStmt *>(stmt.release()));
    if (wrapped->block != nullptr) {
      return std::move(wrapped->block);
    }
    return block;
  }

  block->statements.push_back(std::move(stmt));
  return block;
}

}  // namespace

LoweringResult ASTLowerer::lower(const ast::TranslationUnit &unit) {
  errors_.clear();
  variable_scopes_.clear();
  global_variables_.clear();
  function_signatures_.clear();

  for (const auto &global : unit.global_vars) {
    sema::TypeKind global_type = get_ast_type(*global);
    if (global_type == sema::TypeKind::Unresolved) {
      global_type = resolve_type_name(global->type_name);
    }
    global_variables_[global->name] = global_type;
  }

  for (const auto &function : unit.functions) {
    FunctionSignature signature;
    signature.return_type = get_ast_type(*function);
    if (signature.return_type == sema::TypeKind::Unresolved) {
      signature.return_type = resolve_type_name(function->return_type);
    }

    signature.parameter_types.reserve(function->parameters.size());
    for (const auto &param : function->parameters) {
      sema::TypeKind param_type = get_ast_type(*param);
      if (param_type == sema::TypeKind::Unresolved) {
        param_type = resolve_type_name(param->type_name);
      }
      signature.parameter_types.push_back(param_type);
    }

    function_signatures_.emplace(function->name, signature);
  }

  auto module = std::make_unique<Module>();
  for (const auto &function : unit.functions) {
    auto lowered = lower_function(*function);
    if (lowered != nullptr) {
      module->functions.push_back(std::move(lowered));
    }
  }

  LoweringResult result;
  result.module = std::move(module);
  result.errors = std::move(errors_);
  return result;
}

void ASTLowerer::add_error(parser::SourceLocation location, std::string message) {
  errors_.push_back(LoweringError{location, std::move(message)});
}

std::unique_ptr<Function> ASTLowerer::lower_function(const ast::FunctionDecl &function) {
  sema::TypeKind return_type = sema::TypeKind::Unresolved;
  const auto found = function_signatures_.find(function.name);
  if (found != function_signatures_.end()) {
    return_type = found->second.return_type;
  }
  if (return_type == sema::TypeKind::Unresolved) {
    return_type = resolve_type_name(function.return_type);
  }

  auto lowered = std::make_unique<Function>(function.name, return_type);
  current_return_type_ = return_type;

  push_scope();

  for (const auto &param : function.parameters) {
    sema::TypeKind param_type = get_ast_type(*param);
    if (param_type == sema::TypeKind::Unresolved) {
      param_type = resolve_type_name(param->type_name);
    }
    define_variable(param->name, param_type);
    lowered->parameters.push_back({param->name, param_type});
  }

  if (function.body != nullptr) {
    lowered->entry = lower_block(*function.body, false);
  } else {
    lowered->entry = std::make_unique<Block>();
  }

  pop_scope();
  return lowered;
}

std::unique_ptr<Block> ASTLowerer::lower_block(const ast::BlockStmt &block, bool create_scope) {
  if (create_scope) {
    push_scope();
  }

  auto lowered = std::make_unique<Block>();
  for (const auto &statement : block.statements) {
    auto lowered_stmt = lower_stmt(*statement);
    if (lowered_stmt != nullptr) {
      lowered->statements.push_back(std::move(lowered_stmt));
    }
  }

  if (create_scope) {
    pop_scope();
  }

  return lowered;
}

std::unique_ptr<Stmt> ASTLowerer::lower_stmt(const ast::Stmt &stmt) {
  switch (stmt.kind) {
    case ast::NodeKind::VarDecl: {
      const auto &decl = *as<ast::VarDecl>(&stmt);
      sema::TypeKind declared_type = get_ast_type(decl);
      if (declared_type == sema::TypeKind::Unresolved) {
        declared_type = resolve_type_name(decl.type_name);
      }

      define_variable(decl.name, declared_type);

      if (decl.is_array && declared_type == sema::TypeKind::BufferData) {
        // Shared/local arrays are modeled as buffer-like indexed storage in this prototype.
        return nullptr;
      }

      auto lowered = std::make_unique<VarDecl>(decl.name, declared_type);
      if (decl.initializer != nullptr) {
        lowered->initializer = lower_expr(*decl.initializer);
        lowered->initializer = cast_if_needed(std::move(lowered->initializer), declared_type);
      }
      return lowered;
    }
    case ast::NodeKind::BlockStmt: {
      const auto &block = *as<ast::BlockStmt>(&stmt);
      return std::make_unique<BlockStmt>(lower_block(block, true));
    }
    case ast::NodeKind::IfStmt: {
      const auto &if_stmt = *as<ast::IfStmt>(&stmt);
      auto lowered = std::make_unique<If>();

      if (if_stmt.condition != nullptr) {
        lowered->condition = lower_expr(*if_stmt.condition);
        lowered->condition = cast_if_needed(std::move(lowered->condition), sema::TypeKind::Bool);
      }

      if (if_stmt.then_branch != nullptr) {
        lowered->then_block = stmt_to_block(lower_stmt(*if_stmt.then_branch));
      } else {
        lowered->then_block = std::make_unique<Block>();
      }

      if (if_stmt.else_branch != nullptr) {
        lowered->else_block = stmt_to_block(lower_stmt(*if_stmt.else_branch));
      }

      return lowered;
    }
    case ast::NodeKind::ForStmt: {
      const auto &for_stmt = *as<ast::ForStmt>(&stmt);
      auto lowered = std::make_unique<Loop>();

      push_scope();

      if (for_stmt.init_stmt != nullptr) {
        lowered->init = lower_stmt(*for_stmt.init_stmt);
      } else if (for_stmt.init_expr != nullptr) {
        lowered->init = std::make_unique<ExprStmt>(lower_expr(*for_stmt.init_expr));
      }

      if (for_stmt.condition != nullptr) {
        lowered->condition = lower_expr(*for_stmt.condition);
        lowered->condition = cast_if_needed(std::move(lowered->condition), sema::TypeKind::Bool);
      }

      if (for_stmt.update != nullptr) {
        lowered->update = lower_expr(*for_stmt.update);
      }

      if (for_stmt.body != nullptr) {
        lowered->body = stmt_to_block(lower_stmt(*for_stmt.body));
      } else {
        lowered->body = std::make_unique<Block>();
      }

      pop_scope();
      return lowered;
    }
    case ast::NodeKind::ReturnStmt: {
      const auto &ret = *as<ast::ReturnStmt>(&stmt);
      auto lowered = std::make_unique<Return>();

      if (ret.value != nullptr) {
        lowered->value = lower_expr(*ret.value);
        lowered->value = cast_if_needed(std::move(lowered->value), current_return_type_);
      }

      return lowered;
    }
    case ast::NodeKind::ExprStmt: {
      const auto &expr_stmt = *as<ast::ExprStmt>(&stmt);
      auto lowered = std::make_unique<ExprStmt>(expr_stmt.expression != nullptr
                                                    ? lower_expr(*expr_stmt.expression)
                                                    : nullptr);
      return lowered;
    }
    default:
      add_error(stmt.location, "unsupported AST statement in HIR lowering");
      return nullptr;
  }
}

std::unique_ptr<Expr> ASTLowerer::lower_expr(const ast::Expr &expr) {
  switch (expr.kind) {
    case ast::NodeKind::LiteralExpr: {
      const auto &literal = *as<ast::LiteralExpr>(&expr);
      if (literal.value_kind == ast::LiteralExpr::ValueKind::Number) {
        sema::TypeKind type = get_ast_type(literal);
        if (type == sema::TypeKind::Unresolved) {
          type = type_system_.numeric_literal_type(literal.text);
        }
        return std::make_unique<Literal>(literal.text, type);
      }

      if (literal.text == "gl_GlobalInvocationID") {
        sema::TypeKind type = get_ast_type(literal);
        if (type == sema::TypeKind::Unresolved) {
          type = sema::TypeKind::Vec3;
        }
        return std::make_unique<BuiltinLoad>(literal.text, "", type);
      }

      if (literal.text == "gl_LocalInvocationID") {
        sema::TypeKind type = get_ast_type(literal);
        if (type == sema::TypeKind::Unresolved) {
          type = sema::TypeKind::Vec3;
        }
        return std::make_unique<BuiltinLoad>(literal.text, "", type);
      }

      sema::TypeKind type = lookup_variable(literal.text);
      if (type == sema::TypeKind::Unresolved) {
        type = get_ast_type(literal);
      }
      return std::make_unique<Variable>(literal.text, type);
    }
    case ast::NodeKind::MemberExpr: {
      const auto &member = *as<ast::MemberExpr>(&expr);

      std::unique_ptr<Expr> object = nullptr;
      if (member.object != nullptr) {
        object = lower_expr(*member.object);
      }

      sema::TypeKind result_type = get_ast_type(member);
      if (result_type == sema::TypeKind::Unresolved) {
        result_type = sema::TypeKind::Error;
      }

      if (object != nullptr && object->kind == NodeKind::BuiltinLoad) {
        const auto *builtin = static_cast<const BuiltinLoad *>(object.get());
        if (builtin->builtin_name == "gl_GlobalInvocationID" ||
            builtin->builtin_name == "gl_LocalInvocationID") {
          return std::make_unique<BuiltinLoad>(builtin->builtin_name, member.member_name, result_type);
        }
      }

      if (object != nullptr && object->kind == NodeKind::Variable) {
        const auto *var = static_cast<const Variable *>(object.get());
        auto load = std::make_unique<BufferLoad>(var->name, member.member_name, result_type);
        return load;
      }

      auto fallback = std::make_unique<Call>("__member." + member.member_name, result_type);
      if (object != nullptr) {
        fallback->args.push_back(std::move(object));
      }
      return fallback;
    }
    case ast::NodeKind::IndexExpr: {
      const auto &index = *as<ast::IndexExpr>(&expr);
      std::unique_ptr<Expr> object = index.object != nullptr ? lower_expr(*index.object) : nullptr;
      std::unique_ptr<Expr> idx = index.index != nullptr ? lower_expr(*index.index) : nullptr;

      sema::TypeKind result_type = get_ast_type(index);
      if (result_type == sema::TypeKind::Unresolved) {
        result_type = sema::TypeKind::Error;
      }

      if (object != nullptr && object->kind == NodeKind::BufferLoad) {
        auto load = std::unique_ptr<BufferLoad>(static_cast<BufferLoad *>(object.release()));
        load->index = cast_if_needed(std::move(idx), sema::TypeKind::Int);
        load->type = result_type;
        return load;
      }

      if (object != nullptr && object->kind == NodeKind::Variable &&
          object->type == sema::TypeKind::BufferData) {
        const auto *var = static_cast<const Variable *>(object.get());
        auto load = std::make_unique<BufferLoad>(var->name, "", result_type);
        load->index = cast_if_needed(std::move(idx), sema::TypeKind::Int);
        return load;
      }

      auto fallback = std::make_unique<Call>("__index", result_type);
      if (object != nullptr) {
        fallback->args.push_back(std::move(object));
      }
      if (idx != nullptr) {
        fallback->args.push_back(std::move(idx));
      }
      return fallback;
    }
    case ast::NodeKind::BinaryExpr: {
      const auto &binary = *as<ast::BinaryExpr>(&expr);

      auto lhs = binary.lhs != nullptr ? lower_expr(*binary.lhs) : nullptr;
      auto rhs = binary.rhs != nullptr ? lower_expr(*binary.rhs) : nullptr;

      sema::TypeKind result_type = get_ast_type(binary);

      if (binary.op == "=") {
        if (lhs != nullptr) {
          rhs = cast_if_needed(std::move(rhs), lhs->type);
          result_type = lhs->type;
        }
      } else {
        if (result_type != sema::TypeKind::Unresolved) {
          lhs = cast_if_needed(std::move(lhs), result_type);
          rhs = cast_if_needed(std::move(rhs), result_type);
        }
      }

      return std::make_unique<Binary>(binary.op, std::move(lhs), std::move(rhs), result_type);
    }
    case ast::NodeKind::CallExpr: {
      const auto &call = *as<ast::CallExpr>(&expr);

      std::string callee_name;
      if (call.callee != nullptr) {
        if (const auto *callee_literal = as<ast::LiteralExpr>(call.callee.get());
            callee_literal != nullptr &&
            callee_literal->value_kind == ast::LiteralExpr::ValueKind::Identifier) {
          callee_name = callee_literal->text;
        }
      }

      if (is_vector_constructor_name(callee_name)) {
        const sema::TypeKind vector_type = resolve_type_name(callee_name);
        auto lowered = std::make_unique<Call>("__ctor." + callee_name, vector_type);

        for (const auto &arg : call.args) {
          auto lowered_arg = lower_expr(*arg);
          lowered_arg = cast_if_needed(std::move(lowered_arg), sema::TypeKind::Float);
          lowered->args.push_back(std::move(lowered_arg));
        }

        return lowered;
      }

      if (callee_name == "bufferLoad") {
        auto lowered = std::make_unique<BufferLoad>("<unknown>", "", get_ast_type(call));
        if (!call.args.empty()) {
          if (const auto *name = as<ast::LiteralExpr>(call.args[0].get());
              name != nullptr && name->value_kind == ast::LiteralExpr::ValueKind::Identifier) {
            lowered->buffer_name = name->text;
          }
        }
        if (call.args.size() >= 2) {
          lowered->index = lower_expr(*call.args[1]);
        }
        return lowered;
      }

      if (callee_name == "bufferStore") {
        auto lowered = std::make_unique<BufferStore>("<unknown>", "");
        if (!call.args.empty()) {
          if (const auto *name = as<ast::LiteralExpr>(call.args[0].get());
              name != nullptr && name->value_kind == ast::LiteralExpr::ValueKind::Identifier) {
            lowered->buffer_name = name->text;
          }
        }
        if (call.args.size() >= 2) {
          lowered->index = lower_expr(*call.args[1]);
        }
        if (call.args.size() >= 3) {
          lowered->value = lower_expr(*call.args[2]);
        }
        return lowered;
      }

      sema::TypeKind result_type = get_ast_type(call);
      if (result_type == sema::TypeKind::Unresolved) {
        const auto function_found = function_signatures_.find(callee_name);
        if (function_found != function_signatures_.end()) {
          result_type = function_found->second.return_type;
        }
      }

      auto lowered = std::make_unique<Call>(callee_name.empty() ? "__indirect" : callee_name,
                                            result_type);

      const auto signature = function_signatures_.find(callee_name);
      for (std::size_t i = 0; i < call.args.size(); ++i) {
        auto lowered_arg = lower_expr(*call.args[i]);

        if (signature != function_signatures_.end() && i < signature->second.parameter_types.size()) {
          lowered_arg = cast_if_needed(std::move(lowered_arg), signature->second.parameter_types[i]);
        }

        lowered->args.push_back(std::move(lowered_arg));
      }

      return lowered;
    }
    default:
      add_error(expr.location, "unsupported AST expression in HIR lowering");
      return std::make_unique<Literal>("<error>", sema::TypeKind::Error);
  }
}

std::unique_ptr<Expr> ASTLowerer::cast_if_needed(std::unique_ptr<Expr> value,
                                                 sema::TypeKind target_type) {
  if (value == nullptr) {
    return value;
  }

  if (target_type == sema::TypeKind::Unresolved || target_type == sema::TypeKind::Error) {
    return value;
  }

  if (value->type == target_type) {
    return value;
  }

  if (!can_implicit_cast(value->type, target_type)) {
    return value;
  }

  const sema::TypeKind from_type = value->type;
  return std::make_unique<Cast>(std::move(value), from_type, target_type);
}

sema::TypeKind ASTLowerer::get_ast_type(const ast::Node &node) const {
  return node.resolved_type;
}

sema::TypeKind ASTLowerer::resolve_type_name(const std::string &name) const {
  return type_system_.resolve_type_name(name);
}

bool ASTLowerer::can_implicit_cast(sema::TypeKind from, sema::TypeKind to) {
  if (from == to) {
    return true;
  }

  if (from == sema::TypeKind::Unresolved || to == sema::TypeKind::Unresolved ||
      from == sema::TypeKind::Error || to == sema::TypeKind::Error) {
    return false;
  }

  if (to == sema::TypeKind::Bool && sema::is_numeric_type(from)) {
    return true;
  }

  if (sema::is_scalar_type(from) && sema::is_scalar_type(to)) {
    return true;
  }

  return false;
}

void ASTLowerer::push_scope() {
  variable_scopes_.emplace_back();
}

void ASTLowerer::pop_scope() {
  if (!variable_scopes_.empty()) {
    variable_scopes_.pop_back();
  }
}

void ASTLowerer::define_variable(const std::string &name, sema::TypeKind type) {
  if (variable_scopes_.empty()) {
    push_scope();
  }
  variable_scopes_.back()[name] = type;
}

sema::TypeKind ASTLowerer::lookup_variable(const std::string &name) const {
  for (auto it = variable_scopes_.rbegin(); it != variable_scopes_.rend(); ++it) {
    const auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }

  const auto global = global_variables_.find(name);
  if (global != global_variables_.end()) {
    return global->second;
  }

  if (name == "gl_GlobalInvocationID") {
    return sema::TypeKind::Vec3;
  }
  if (name == "gl_LocalInvocationID") {
    return sema::TypeKind::Vec3;
  }

  return sema::TypeKind::Unresolved;
}

}  // namespace glsl2llvm::hir
