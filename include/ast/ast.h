#pragma once

#include "parser/token.h"
#include "sema/type.h"

#include <memory>
#include <string>
#include <vector>

namespace glsl2llvm::ast {

enum class NodeKind {
  TranslationUnit,
  FunctionDecl,
  VarDecl,
  BlockStmt,
  IfStmt,
  ForStmt,
  ReturnStmt,
  ExprStmt,
  BinaryExpr,
  CallExpr,
  MemberExpr,
  IndexExpr,
  LiteralExpr,
};

struct Node {
  Node(NodeKind node_kind, parser::SourceLocation node_location)
      : kind(node_kind), location(node_location) {}
  virtual ~Node() = default;

  NodeKind kind;
  parser::SourceLocation location;
  sema::TypeKind resolved_type = sema::TypeKind::Unresolved;
};

struct Expr : Node {
 protected:
  Expr(NodeKind node_kind, parser::SourceLocation node_location) : Node(node_kind, node_location) {}
};

struct Stmt : Node {
 protected:
  Stmt(NodeKind node_kind, parser::SourceLocation node_location) : Node(node_kind, node_location) {}
};

struct LiteralExpr final : Expr {
  enum class ValueKind {
    Number,
    Identifier,
  };

  LiteralExpr(parser::SourceLocation node_location, ValueKind literal_kind, std::string literal_text)
      : Expr(NodeKind::LiteralExpr, node_location),
        value_kind(literal_kind),
        text(std::move(literal_text)) {}

  ValueKind value_kind;
  std::string text;
};

struct BinaryExpr final : Expr {
  BinaryExpr(parser::SourceLocation node_location, std::string binary_op, std::unique_ptr<Expr> lhs_expr,
             std::unique_ptr<Expr> rhs_expr)
      : Expr(NodeKind::BinaryExpr, node_location),
        op(std::move(binary_op)),
        lhs(std::move(lhs_expr)),
        rhs(std::move(rhs_expr)) {}

  std::string op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
};

struct CallExpr final : Expr {
  CallExpr(parser::SourceLocation node_location, std::unique_ptr<Expr> callee_expr)
      : Expr(NodeKind::CallExpr, node_location), callee(std::move(callee_expr)) {}

  std::unique_ptr<Expr> callee;
  std::vector<std::unique_ptr<Expr>> args;
};

struct MemberExpr final : Expr {
  MemberExpr(parser::SourceLocation node_location, std::unique_ptr<Expr> object_expr, std::string member)
      : Expr(NodeKind::MemberExpr, node_location),
        object(std::move(object_expr)),
        member_name(std::move(member)) {}

  std::unique_ptr<Expr> object;
  std::string member_name;
};

struct IndexExpr final : Expr {
  IndexExpr(parser::SourceLocation node_location, std::unique_ptr<Expr> object_expr,
            std::unique_ptr<Expr> index_expr)
      : Expr(NodeKind::IndexExpr, node_location),
        object(std::move(object_expr)),
        index(std::move(index_expr)) {}

  std::unique_ptr<Expr> object;
  std::unique_ptr<Expr> index;
};

struct VarDecl final : Stmt {
  VarDecl(parser::SourceLocation node_location, std::string var_type, std::string var_name,
          std::unique_ptr<Expr> var_initializer = nullptr)
      : Stmt(NodeKind::VarDecl, node_location),
        type_name(std::move(var_type)),
        name(std::move(var_name)),
        initializer(std::move(var_initializer)) {}

  std::string type_name;
  std::string name;
  std::unique_ptr<Expr> initializer;
  bool is_parameter = false;
  bool is_buffer_block = false;
  std::string buffer_field_name;
  std::string buffer_element_type;
};

struct BlockStmt final : Stmt {
  explicit BlockStmt(parser::SourceLocation node_location) : Stmt(NodeKind::BlockStmt, node_location) {}

  std::vector<std::unique_ptr<Stmt>> statements;
};

struct IfStmt final : Stmt {
  IfStmt(parser::SourceLocation node_location, std::unique_ptr<Expr> cond,
         std::unique_ptr<Stmt> then_stmt)
      : Stmt(NodeKind::IfStmt, node_location),
        condition(std::move(cond)),
        then_branch(std::move(then_stmt)) {}

  std::unique_ptr<Expr> condition;
  std::unique_ptr<Stmt> then_branch;
};

struct ForStmt final : Stmt {
  explicit ForStmt(parser::SourceLocation node_location) : Stmt(NodeKind::ForStmt, node_location) {}

  std::unique_ptr<Stmt> init_stmt;
  std::unique_ptr<Expr> init_expr;
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Expr> update;
  std::unique_ptr<Stmt> body;
};

struct ReturnStmt final : Stmt {
  ReturnStmt(parser::SourceLocation node_location, std::unique_ptr<Expr> return_value)
      : Stmt(NodeKind::ReturnStmt, node_location), value(std::move(return_value)) {}

  std::unique_ptr<Expr> value;
};

struct ExprStmt final : Stmt {
  ExprStmt(parser::SourceLocation node_location, std::unique_ptr<Expr> expr)
      : Stmt(NodeKind::ExprStmt, node_location), expression(std::move(expr)) {}

  std::unique_ptr<Expr> expression;
};

struct FunctionDecl final : Node {
  FunctionDecl(parser::SourceLocation node_location, std::string type_name, std::string function_name)
      : Node(NodeKind::FunctionDecl, node_location),
        return_type(std::move(type_name)),
        name(std::move(function_name)) {}

  std::string return_type;
  std::string name;
  std::vector<std::unique_ptr<VarDecl>> parameters;
  std::unique_ptr<BlockStmt> body;
};

struct TranslationUnit final : Node {
  explicit TranslationUnit(parser::SourceLocation node_location)
      : Node(NodeKind::TranslationUnit, node_location) {}

  std::vector<std::unique_ptr<FunctionDecl>> functions;
  std::vector<std::unique_ptr<VarDecl>> global_vars;
};

std::string dump(const TranslationUnit &unit);

}  // namespace glsl2llvm::ast
