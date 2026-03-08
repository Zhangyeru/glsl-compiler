#include "hir/hir.h"

#include <sstream>

namespace glsl2llvm::hir {

namespace {

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
    case NodeKind::Literal: {
      const auto *node = static_cast<const Literal *>(expr);
      indent(out, depth);
      out << "HIRLiteral value='" << node->value << "' type=" << sema::to_string(node->type)
          << "\n";
      return;
    }
    case NodeKind::Variable: {
      const auto *node = static_cast<const Variable *>(expr);
      indent(out, depth);
      out << "HIRVariable name='" << node->name << "' type=" << sema::to_string(node->type)
          << "\n";
      return;
    }
    case NodeKind::Binary: {
      const auto *node = static_cast<const Binary *>(expr);
      indent(out, depth);
      out << "HIRBinary op='" << node->op << "' type=" << sema::to_string(node->type) << "\n";
      dump_expr(node->lhs.get(), out, depth + 1);
      dump_expr(node->rhs.get(), out, depth + 1);
      return;
    }
    case NodeKind::Call: {
      const auto *node = static_cast<const Call *>(expr);
      indent(out, depth);
      out << "HIRCall callee='" << node->callee << "' type=" << sema::to_string(node->type)
          << "\n";
      for (const auto &arg : node->args) {
        dump_expr(arg.get(), out, depth + 1);
      }
      return;
    }
    case NodeKind::BuiltinLoad: {
      const auto *node = static_cast<const BuiltinLoad *>(expr);
      indent(out, depth);
      out << "HIRBuiltinLoad builtin='" << node->builtin_name << "' component='" << node->component
          << "' type=" << sema::to_string(node->type) << "\n";
      return;
    }
    case NodeKind::BufferLoad: {
      const auto *node = static_cast<const BufferLoad *>(expr);
      indent(out, depth);
      out << "HIRBufferLoad buffer='" << node->buffer_name << "' field='" << node->field_name
          << "' type=" << sema::to_string(node->type) << "\n";
      dump_expr(node->index.get(), out, depth + 1);
      return;
    }
    case NodeKind::BufferStore: {
      const auto *node = static_cast<const BufferStore *>(expr);
      indent(out, depth);
      out << "HIRBufferStore buffer='" << node->buffer_name << "' field='" << node->field_name
          << "'\n";
      dump_expr(node->index.get(), out, depth + 1);
      dump_expr(node->value.get(), out, depth + 1);
      return;
    }
    case NodeKind::Cast: {
      const auto *node = static_cast<const Cast *>(expr);
      indent(out, depth);
      out << "HIRCast from=" << sema::to_string(node->from_type)
          << " to=" << sema::to_string(node->to_type) << "\n";
      dump_expr(node->value.get(), out, depth + 1);
      return;
    }
    default:
      indent(out, depth);
      out << "<unexpected expr node>\n";
      return;
  }
}

void dump_block(const Block *block, std::ostringstream &out, int depth) {
  if (block == nullptr) {
    indent(out, depth);
    out << "<null block>\n";
    return;
  }

  indent(out, depth);
  out << "HIRBlock\n";
  for (const auto &stmt : block->statements) {
    dump_stmt(stmt.get(), out, depth + 1);
  }
}

void dump_stmt(const Stmt *stmt, std::ostringstream &out, int depth) {
  if (stmt == nullptr) {
    indent(out, depth);
    out << "<null stmt>\n";
    return;
  }

  switch (stmt->kind) {
    case NodeKind::ExprStmt: {
      const auto *node = static_cast<const ExprStmt *>(stmt);
      indent(out, depth);
      out << "HIRExprStmt\n";
      dump_expr(node->expression.get(), out, depth + 1);
      return;
    }
    case NodeKind::VarDecl: {
      const auto *node = static_cast<const VarDecl *>(stmt);
      indent(out, depth);
      out << "HIRVarDecl name='" << node->name << "' type=" << sema::to_string(node->declared_type)
          << "\n";
      dump_expr(node->initializer.get(), out, depth + 1);
      return;
    }
    case NodeKind::Return: {
      const auto *node = static_cast<const Return *>(stmt);
      indent(out, depth);
      out << "HIRReturn\n";
      dump_expr(node->value.get(), out, depth + 1);
      return;
    }
    case NodeKind::BlockStmt: {
      const auto *node = static_cast<const BlockStmt *>(stmt);
      indent(out, depth);
      out << "HIRBlockStmt\n";
      dump_block(node->block.get(), out, depth + 1);
      return;
    }
    case NodeKind::If: {
      const auto *node = static_cast<const If *>(stmt);
      indent(out, depth);
      out << "HIRIf\n";
      indent(out, depth + 1);
      out << "Condition:\n";
      dump_expr(node->condition.get(), out, depth + 2);
      indent(out, depth + 1);
      out << "Then:\n";
      dump_block(node->then_block.get(), out, depth + 2);
      if (node->else_block != nullptr) {
        indent(out, depth + 1);
        out << "Else:\n";
        dump_block(node->else_block.get(), out, depth + 2);
      }
      return;
    }
    case NodeKind::Loop: {
      const auto *node = static_cast<const Loop *>(stmt);
      indent(out, depth);
      out << "HIRLoop\n";
      indent(out, depth + 1);
      out << "Init:\n";
      dump_stmt(node->init.get(), out, depth + 2);
      indent(out, depth + 1);
      out << "Condition:\n";
      dump_expr(node->condition.get(), out, depth + 2);
      indent(out, depth + 1);
      out << "Update:\n";
      dump_expr(node->update.get(), out, depth + 2);
      indent(out, depth + 1);
      out << "Body:\n";
      dump_block(node->body.get(), out, depth + 2);
      return;
    }
    default:
      indent(out, depth);
      out << "<unexpected stmt node>\n";
      return;
  }
}

}  // namespace

std::string dump(const Module &module) {
  std::ostringstream out;

  for (const auto &function : module.functions) {
    out << "HIRFunction name='" << function->name << "' return="
        << sema::to_string(function->return_type) << "\n";

    out << "  Params(" << function->parameters.size() << ")\n";
    for (const auto &param : function->parameters) {
      out << "    " << param.first << ":" << sema::to_string(param.second) << "\n";
    }

    dump_block(function->entry.get(), out, 1);
  }

  return out.str();
}

}  // namespace glsl2llvm::hir
