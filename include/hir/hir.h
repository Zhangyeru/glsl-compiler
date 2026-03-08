#pragma once

#include "sema/type.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace glsl2llvm::hir {

enum class NodeKind {
  Function,
  Block,
  If,
  Loop,
  Binary,
  Call,
  BuiltinLoad,
  BufferLoad,
  BufferStore,
  Cast,

  ExprStmt,
  VarDecl,
  Return,
  BlockStmt,
  Literal,
  Variable,
};

struct Node {
  Node(NodeKind node_kind, sema::TypeKind node_type) : kind(node_kind), type(node_type) {}
  virtual ~Node() = default;

  NodeKind kind;
  sema::TypeKind type;
};

struct Expr : Node {
 protected:
  Expr(NodeKind node_kind, sema::TypeKind node_type) : Node(node_kind, node_type) {}
};

struct Stmt : Node {
 protected:
  Stmt(NodeKind node_kind, sema::TypeKind node_type) : Node(node_kind, node_type) {}
};

struct Literal final : Expr {
  explicit Literal(std::string literal_value, sema::TypeKind literal_type)
      : Expr(NodeKind::Literal, literal_type), value(std::move(literal_value)) {}

  std::string value;
};

struct Variable final : Expr {
  explicit Variable(std::string var_name, sema::TypeKind var_type)
      : Expr(NodeKind::Variable, var_type), name(std::move(var_name)) {}

  std::string name;
};

struct Binary final : Expr {
  Binary(std::string binary_op, std::unique_ptr<Expr> lhs_expr, std::unique_ptr<Expr> rhs_expr,
         sema::TypeKind result_type)
      : Expr(NodeKind::Binary, result_type),
        op(std::move(binary_op)),
        lhs(std::move(lhs_expr)),
        rhs(std::move(rhs_expr)) {}

  std::string op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
};

struct Call final : Expr {
  Call(std::string callee_name, sema::TypeKind result_type)
      : Expr(NodeKind::Call, result_type), callee(std::move(callee_name)) {}

  std::string callee;
  std::vector<std::unique_ptr<Expr>> args;
};

struct BuiltinLoad final : Expr {
  BuiltinLoad(std::string name, std::string component_name, sema::TypeKind result_type)
      : Expr(NodeKind::BuiltinLoad, result_type),
        builtin_name(std::move(name)),
        component(std::move(component_name)) {}

  std::string builtin_name;
  std::string component;
};

struct BufferLoad final : Expr {
  BufferLoad(std::string name, std::string field, sema::TypeKind result_type)
      : Expr(NodeKind::BufferLoad, result_type),
        buffer_name(std::move(name)),
        field_name(std::move(field)) {}

  std::string buffer_name;
  std::string field_name;
  std::unique_ptr<Expr> index;
};

struct Cast final : Expr {
  Cast(std::unique_ptr<Expr> cast_value, sema::TypeKind from, sema::TypeKind to)
      : Expr(NodeKind::Cast, to), value(std::move(cast_value)), from_type(from), to_type(to) {}

  std::unique_ptr<Expr> value;
  sema::TypeKind from_type;
  sema::TypeKind to_type;
};

struct BufferStore final : Expr {
  BufferStore(std::string name, std::string field)
      : Expr(NodeKind::BufferStore, sema::TypeKind::Void),
        buffer_name(std::move(name)),
        field_name(std::move(field)) {}

  std::string buffer_name;
  std::string field_name;
  std::unique_ptr<Expr> index;
  std::unique_ptr<Expr> value;
};

struct ExprStmt final : Stmt {
  explicit ExprStmt(std::unique_ptr<Expr> expr)
      : Stmt(NodeKind::ExprStmt, sema::TypeKind::Void), expression(std::move(expr)) {}

  std::unique_ptr<Expr> expression;
};

struct VarDecl final : Stmt {
  VarDecl(std::string var_name, sema::TypeKind var_type)
      : Stmt(NodeKind::VarDecl, sema::TypeKind::Void),
        name(std::move(var_name)),
        declared_type(var_type) {}

  std::string name;
  sema::TypeKind declared_type;
  std::unique_ptr<Expr> initializer;
};

struct Return final : Stmt {
  Return() : Stmt(NodeKind::Return, sema::TypeKind::Void) {}

  std::unique_ptr<Expr> value;
};

struct Block;

struct BlockStmt final : Stmt {
  explicit BlockStmt(std::unique_ptr<Block> nested)
      : Stmt(NodeKind::BlockStmt, sema::TypeKind::Void), block(std::move(nested)) {}

  std::unique_ptr<Block> block;
};

struct If final : Stmt {
  If() : Stmt(NodeKind::If, sema::TypeKind::Void) {}

  std::unique_ptr<Expr> condition;
  std::unique_ptr<Block> then_block;
  std::unique_ptr<Block> else_block;
};

struct Loop final : Stmt {
  Loop() : Stmt(NodeKind::Loop, sema::TypeKind::Void) {}

  std::unique_ptr<Stmt> init;
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Expr> update;
  std::unique_ptr<Block> body;
};

struct Block final : Node {
  Block() : Node(NodeKind::Block, sema::TypeKind::Void) {}

  std::vector<std::unique_ptr<Stmt>> statements;
};

struct Function final : Node {
  Function(std::string function_name, sema::TypeKind ret_type)
      : Node(NodeKind::Function, ret_type), name(std::move(function_name)), return_type(ret_type) {}

  std::string name;
  sema::TypeKind return_type;
  std::vector<std::pair<std::string, sema::TypeKind>> parameters;
  std::unique_ptr<Block> entry;
};

struct Module {
  std::vector<std::unique_ptr<Function>> functions;
};

std::string dump(const Module &module);

}  // namespace glsl2llvm::hir
