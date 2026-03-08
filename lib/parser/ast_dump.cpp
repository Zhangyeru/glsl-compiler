#include "ast/ast.h"

#include <sstream>
#include <string>

namespace glsl2llvm::ast {

namespace {

std::string loc(const parser::SourceLocation &location) {
  return std::to_string(location.line) + ":" + std::to_string(location.column);
}

void indent(std::ostringstream &out, int depth) {
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
}

void dump_expr(const Expr *expr, std::ostringstream &out, int depth);
void dump_stmt(const Stmt *stmt, std::ostringstream &out, int depth);

void dump_expr(const Expr *expr, std::ostringstream &out, int depth) {
  if (expr == nullptr) {
    indent(out, depth);
    out << "<null expr>\n";
    return;
  }

  switch (expr->kind) {
    case NodeKind::LiteralExpr: {
      const auto *literal = static_cast<const LiteralExpr *>(expr);
      indent(out, depth);
      out << "LiteralExpr kind="
          << (literal->value_kind == LiteralExpr::ValueKind::Number ? "number" : "identifier")
          << " text='" << literal->text << "' @" << loc(literal->location) << "\n";
      return;
    }
    case NodeKind::BinaryExpr: {
      const auto *binary = static_cast<const BinaryExpr *>(expr);
      indent(out, depth);
      out << "BinaryExpr op='" << binary->op << "' @" << loc(binary->location) << "\n";
      dump_expr(binary->lhs.get(), out, depth + 1);
      dump_expr(binary->rhs.get(), out, depth + 1);
      return;
    }
    case NodeKind::CallExpr: {
      const auto *call = static_cast<const CallExpr *>(expr);
      indent(out, depth);
      out << "CallExpr @" << loc(call->location) << "\n";
      indent(out, depth + 1);
      out << "Callee:\n";
      dump_expr(call->callee.get(), out, depth + 2);
      indent(out, depth + 1);
      out << "Args(" << call->args.size() << "):\n";
      for (const auto &arg : call->args) {
        dump_expr(arg.get(), out, depth + 2);
      }
      return;
    }
    case NodeKind::MemberExpr: {
      const auto *member = static_cast<const MemberExpr *>(expr);
      indent(out, depth);
      out << "MemberExpr member='" << member->member_name << "' @" << loc(member->location)
          << "\n";
      dump_expr(member->object.get(), out, depth + 1);
      return;
    }
    case NodeKind::IndexExpr: {
      const auto *index = static_cast<const IndexExpr *>(expr);
      indent(out, depth);
      out << "IndexExpr @" << loc(index->location) << "\n";
      indent(out, depth + 1);
      out << "Object:\n";
      dump_expr(index->object.get(), out, depth + 2);
      indent(out, depth + 1);
      out << "Index:\n";
      dump_expr(index->index.get(), out, depth + 2);
      return;
    }
    default:
      indent(out, depth);
      out << "<unexpected expr kind>\n";
      return;
  }
}

void dump_stmt(const Stmt *stmt, std::ostringstream &out, int depth) {
  if (stmt == nullptr) {
    indent(out, depth);
    out << "<null stmt>\n";
    return;
  }

  switch (stmt->kind) {
    case NodeKind::VarDecl: {
      const auto *var = static_cast<const VarDecl *>(stmt);
      indent(out, depth);
      out << "VarDecl type='" << var->type_name << "' name='" << var->name << "' @"
          << loc(var->location) << "\n";
      if (var->is_buffer_block) {
        indent(out, depth + 1);
        out << "BufferBlock field='" << var->buffer_field_name << "' element='"
            << var->buffer_element_type << "'\n";
      }
      if (var->is_shared) {
        indent(out, depth + 1);
        out << "Qualifier: shared\n";
      }
      if (var->is_array) {
        indent(out, depth + 1);
        out << "ArrayDecl: true\n";
      }
      if (var->initializer != nullptr) {
        indent(out, depth + 1);
        out << "Initializer:\n";
        dump_expr(var->initializer.get(), out, depth + 2);
      }
      return;
    }
    case NodeKind::BlockStmt: {
      const auto *block = static_cast<const BlockStmt *>(stmt);
      indent(out, depth);
      out << "BlockStmt @" << loc(block->location) << "\n";
      for (const auto &item : block->statements) {
        dump_stmt(item.get(), out, depth + 1);
      }
      return;
    }
    case NodeKind::IfStmt: {
      const auto *if_stmt = static_cast<const IfStmt *>(stmt);
      indent(out, depth);
      out << "IfStmt @" << loc(if_stmt->location) << "\n";
      indent(out, depth + 1);
      out << "Condition:\n";
      dump_expr(if_stmt->condition.get(), out, depth + 2);
      indent(out, depth + 1);
      out << "Then:\n";
      dump_stmt(if_stmt->then_branch.get(), out, depth + 2);
      if (if_stmt->else_branch != nullptr) {
        indent(out, depth + 1);
        out << "Else:\n";
        dump_stmt(if_stmt->else_branch.get(), out, depth + 2);
      }
      return;
    }
    case NodeKind::ForStmt: {
      const auto *for_stmt = static_cast<const ForStmt *>(stmt);
      indent(out, depth);
      out << "ForStmt @" << loc(for_stmt->location) << "\n";

      indent(out, depth + 1);
      out << "InitStmt:\n";
      dump_stmt(for_stmt->init_stmt.get(), out, depth + 2);

      indent(out, depth + 1);
      out << "InitExpr:\n";
      dump_expr(for_stmt->init_expr.get(), out, depth + 2);

      indent(out, depth + 1);
      out << "Condition:\n";
      dump_expr(for_stmt->condition.get(), out, depth + 2);

      indent(out, depth + 1);
      out << "Update:\n";
      dump_expr(for_stmt->update.get(), out, depth + 2);

      indent(out, depth + 1);
      out << "Body:\n";
      dump_stmt(for_stmt->body.get(), out, depth + 2);
      return;
    }
    case NodeKind::ReturnStmt: {
      const auto *ret = static_cast<const ReturnStmt *>(stmt);
      indent(out, depth);
      out << "ReturnStmt @" << loc(ret->location) << "\n";
      dump_expr(ret->value.get(), out, depth + 1);
      return;
    }
    case NodeKind::ExprStmt: {
      const auto *expr_stmt = static_cast<const ExprStmt *>(stmt);
      indent(out, depth);
      out << "ExprStmt @" << loc(expr_stmt->location) << "\n";
      dump_expr(expr_stmt->expression.get(), out, depth + 1);
      return;
    }
    default:
      indent(out, depth);
      out << "<unexpected stmt kind>\n";
      return;
  }
}

}  // namespace

std::string dump(const TranslationUnit &unit) {
  std::ostringstream out;

  out << "TranslationUnit @" << loc(unit.location) << "\n";

  for (const auto &global : unit.global_vars) {
    dump_stmt(global.get(), out, 1);
  }

  for (const auto &function : unit.functions) {
    indent(out, 1);
    out << "FunctionDecl return='" << function->return_type << "' name='" << function->name
        << "' @" << loc(function->location) << "\n";

    indent(out, 2);
    out << "Params(" << function->parameters.size() << "):\n";
    for (const auto &param : function->parameters) {
      dump_stmt(param.get(), out, 3);
    }

    indent(out, 2);
    out << "Body:\n";
    dump_stmt(function->body.get(), out, 3);
  }

  return out.str();
}

}  // namespace glsl2llvm::ast
