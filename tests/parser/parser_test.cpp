#include "ast/ast.h"
#include "parser/parser.h"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {

using glsl2llvm::ast::BinaryExpr;
using glsl2llvm::ast::BlockStmt;
using glsl2llvm::ast::CallExpr;
using glsl2llvm::ast::ForStmt;
using glsl2llvm::ast::FunctionDecl;
using glsl2llvm::ast::IfStmt;
using glsl2llvm::ast::LiteralExpr;
using glsl2llvm::ast::MemberExpr;
using glsl2llvm::ast::ReturnStmt;
using glsl2llvm::ast::TranslationUnit;
using glsl2llvm::ast::VarDecl;
using glsl2llvm::parser::ParseResult;
using glsl2llvm::parser::Parser;

bool fail(const std::string &message) {
  std::cerr << message << "\n";
  return false;
}

std::string format_errors(const ParseResult &result) {
  std::string out;
  for (const auto &error : result.errors) {
    out += std::to_string(error.location.line) + ":" + std::to_string(error.location.column) +
           " " + error.message + "\n";
  }
  return out;
}

ParseResult parse_source(const std::string &source) {
  Parser parser(source);
  return parser.parse_translation_unit();
}

template <typename T>
const T *as(const glsl2llvm::ast::Node *node) {
  return dynamic_cast<const T *>(node);
}

bool expect_ok(const ParseResult &result, const std::string &label) {
  if (!result.ok()) {
    return fail(label + ": unexpected parser errors:\n" + format_errors(result));
  }
  if (result.unit == nullptr) {
    return fail(label + ": missing translation unit");
  }
  return true;
}

bool expect_binary(const glsl2llvm::ast::Expr *expr, const std::string &op,
                   const BinaryExpr *&out_binary, const std::string &label) {
  out_binary = as<BinaryExpr>(expr);
  if (out_binary == nullptr) {
    return fail(label + ": expected BinaryExpr");
  }
  if (out_binary->op != op) {
    return fail(label + ": expected op '" + op + "', got '" + out_binary->op + "'");
  }
  return true;
}

bool case_function_basic() {
  const ParseResult result = parse_source("void main() { return; }");
  if (!expect_ok(result, "function_basic")) {
    return false;
  }

  const TranslationUnit &unit = *result.unit;
  if (unit.functions.size() != 1) {
    return fail("function_basic: expected 1 function");
  }

  const FunctionDecl &fn = *unit.functions[0];
  if (fn.name != "main" || fn.return_type != "void") {
    return fail("function_basic: unexpected function signature");
  }

  if (fn.body == nullptr || fn.body->statements.size() != 1) {
    return fail("function_basic: expected 1 statement in body");
  }

  const auto *ret = as<ReturnStmt>(fn.body->statements[0].get());
  if (ret == nullptr) {
    return fail("function_basic: expected ReturnStmt");
  }

  return ret->value == nullptr;
}

bool case_function_with_return_binary() {
  const ParseResult result = parse_source(
      "float foo(float x) {\n"
      "  return x * 2.0;\n"
      "}\n");
  if (!expect_ok(result, "function_with_return_binary")) {
    return false;
  }

  const FunctionDecl &fn = *result.unit->functions[0];
  if (fn.name != "foo" || fn.return_type != "float") {
    return fail("function_with_return_binary: bad function signature");
  }
  if (fn.parameters.size() != 1) {
    return fail("function_with_return_binary: expected 1 parameter");
  }
  if (fn.parameters[0]->name != "x" || fn.parameters[0]->type_name != "float") {
    return fail("function_with_return_binary: bad parameter");
  }

  const auto *ret = as<ReturnStmt>(fn.body->statements[0].get());
  if (ret == nullptr || ret->value == nullptr) {
    return fail("function_with_return_binary: expected return expression");
  }

  const BinaryExpr *binary = nullptr;
  if (!expect_binary(ret->value.get(), "*", binary, "function_with_return_binary")) {
    return false;
  }

  const auto *lhs = as<LiteralExpr>(binary->lhs.get());
  const auto *rhs = as<LiteralExpr>(binary->rhs.get());
  if (lhs == nullptr || rhs == nullptr) {
    return fail("function_with_return_binary: expected literal operands");
  }
  if (lhs->text != "x" || rhs->text != "2.0") {
    return fail("function_with_return_binary: unexpected operands");
  }

  return true;
}

bool case_two_functions() {
  const ParseResult result = parse_source(
      "float foo(float x) { return x * 2.0; }\n"
      "void main() { float a = 1.0; }\n");
  if (!expect_ok(result, "two_functions")) {
    return false;
  }

  if (result.unit->functions.size() != 2) {
    return fail("two_functions: expected 2 functions");
  }
  if (result.unit->functions[0]->name != "foo" || result.unit->functions[1]->name != "main") {
    return fail("two_functions: unexpected function names");
  }

  return true;
}

bool case_local_var_init() {
  const ParseResult result = parse_source("void main() { float a = 1.0; }");
  if (!expect_ok(result, "local_var_init")) {
    return false;
  }

  const FunctionDecl &fn = *result.unit->functions[0];
  const auto *var = as<VarDecl>(fn.body->statements[0].get());
  if (var == nullptr) {
    return fail("local_var_init: expected VarDecl");
  }
  if (var->name != "a" || var->type_name != "float") {
    return fail("local_var_init: unexpected declaration");
  }
  const auto *literal = as<LiteralExpr>(var->initializer.get());
  if (literal == nullptr || literal->text != "1.0") {
    return fail("local_var_init: unexpected initializer");
  }

  return true;
}

bool case_local_var_no_init() {
  const ParseResult result = parse_source("void main() { int a; }");
  if (!expect_ok(result, "local_var_no_init")) {
    return false;
  }

  const auto *var = as<VarDecl>(result.unit->functions[0]->body->statements[0].get());
  if (var == nullptr) {
    return fail("local_var_no_init: expected VarDecl");
  }
  if (var->initializer != nullptr) {
    return fail("local_var_no_init: initializer should be null");
  }

  return true;
}

bool case_nested_block() {
  const ParseResult result = parse_source("void main() { { return; } }");
  if (!expect_ok(result, "nested_block")) {
    return false;
  }

  const auto *block = as<BlockStmt>(result.unit->functions[0]->body->statements[0].get());
  if (block == nullptr || block->statements.size() != 1) {
    return fail("nested_block: bad nested block");
  }

  return as<ReturnStmt>(block->statements[0].get()) != nullptr;
}

bool case_if_stmt() {
  const ParseResult result = parse_source("void main() { if (1) { return; } }");
  if (!expect_ok(result, "if_stmt")) {
    return false;
  }

  const auto *if_stmt = as<IfStmt>(result.unit->functions[0]->body->statements[0].get());
  if (if_stmt == nullptr) {
    return fail("if_stmt: expected IfStmt");
  }

  const auto *cond = as<LiteralExpr>(if_stmt->condition.get());
  if (cond == nullptr || cond->text != "1") {
    return fail("if_stmt: bad condition");
  }

  return as<BlockStmt>(if_stmt->then_branch.get()) != nullptr;
}

bool case_for_stmt_var_init() {
  const ParseResult result = parse_source(
      "void main() { for (int i = 0; i; i = i + 1) { return; } }");
  if (!expect_ok(result, "for_stmt_var_init")) {
    return false;
  }

  const auto *for_stmt = as<ForStmt>(result.unit->functions[0]->body->statements[0].get());
  if (for_stmt == nullptr) {
    return fail("for_stmt_var_init: expected ForStmt");
  }

  const auto *init_var = as<VarDecl>(for_stmt->init_stmt.get());
  if (init_var == nullptr || init_var->name != "i" || init_var->type_name != "int") {
    return fail("for_stmt_var_init: bad init var");
  }

  const auto *cond = as<LiteralExpr>(for_stmt->condition.get());
  if (cond == nullptr || cond->text != "i") {
    return fail("for_stmt_var_init: bad condition");
  }

  const BinaryExpr *assign = nullptr;
  if (!expect_binary(for_stmt->update.get(), "=", assign, "for_stmt_var_init update")) {
    return false;
  }
  const BinaryExpr *sum = nullptr;
  if (!expect_binary(assign->rhs.get(), "+", sum, "for_stmt_var_init sum")) {
    return false;
  }

  return true;
}

bool case_for_stmt_expr_init() {
  const ParseResult result = parse_source(
      "void main() { for (i = 0; i; i = i + 1) { return; } }");
  if (!expect_ok(result, "for_stmt_expr_init")) {
    return false;
  }

  const auto *for_stmt = as<ForStmt>(result.unit->functions[0]->body->statements[0].get());
  if (for_stmt == nullptr) {
    return fail("for_stmt_expr_init: expected ForStmt");
  }
  if (for_stmt->init_stmt != nullptr) {
    return fail("for_stmt_expr_init: init_stmt should be null");
  }

  const BinaryExpr *assign = nullptr;
  return expect_binary(for_stmt->init_expr.get(), "=", assign, "for_stmt_expr_init init");
}

bool case_call_expr() {
  const ParseResult result = parse_source(
      "float foo(float x) { return x; }\n"
      "float bar() { return foo(1.0); }\n");
  if (!expect_ok(result, "call_expr")) {
    return false;
  }

  const FunctionDecl &bar = *result.unit->functions[1];
  const auto *ret = as<ReturnStmt>(bar.body->statements[0].get());
  if (ret == nullptr) {
    return fail("call_expr: expected return");
  }

  const auto *call = as<CallExpr>(ret->value.get());
  if (call == nullptr) {
    return fail("call_expr: expected CallExpr");
  }
  if (call->args.size() != 1) {
    return fail("call_expr: expected one arg");
  }

  const auto *callee = as<LiteralExpr>(call->callee.get());
  if (callee == nullptr || callee->text != "foo") {
    return fail("call_expr: bad callee");
  }

  const auto *arg = as<LiteralExpr>(call->args[0].get());
  return arg != nullptr && arg->text == "1.0";
}

bool case_member_expr() {
  const ParseResult result = parse_source("float foo() { return a.b; }");
  if (!expect_ok(result, "member_expr")) {
    return false;
  }

  const auto *ret = as<ReturnStmt>(result.unit->functions[0]->body->statements[0].get());
  if (ret == nullptr) {
    return fail("member_expr: expected return");
  }

  const auto *member = as<MemberExpr>(ret->value.get());
  if (member == nullptr || member->member_name != "b") {
    return fail("member_expr: bad member expression");
  }

  const auto *object = as<LiteralExpr>(member->object.get());
  return object != nullptr && object->text == "a";
}

bool case_precedence_mul_add() {
  const ParseResult result = parse_source("float foo() { return 1 + 2 * 3; }");
  if (!expect_ok(result, "precedence_mul_add")) {
    return false;
  }

  const auto *ret = as<ReturnStmt>(result.unit->functions[0]->body->statements[0].get());
  const BinaryExpr *root = nullptr;
  if (!expect_binary(ret->value.get(), "+", root, "precedence_mul_add root")) {
    return false;
  }

  const BinaryExpr *rhs = nullptr;
  return expect_binary(root->rhs.get(), "*", rhs, "precedence_mul_add rhs");
}

bool case_precedence_paren() {
  const ParseResult result = parse_source("float foo() { return (1 + 2) * 3; }");
  if (!expect_ok(result, "precedence_paren")) {
    return false;
  }

  const auto *ret = as<ReturnStmt>(result.unit->functions[0]->body->statements[0].get());
  const BinaryExpr *root = nullptr;
  if (!expect_binary(ret->value.get(), "*", root, "precedence_paren root")) {
    return false;
  }

  const BinaryExpr *lhs = nullptr;
  return expect_binary(root->lhs.get(), "+", lhs, "precedence_paren lhs");
}

bool case_ast_dump() {
  const ParseResult result = parse_source(
      "float foo(float x) { return x * 2.0; }\n"
      "void main() { float a = 1.0; }\n");
  if (!expect_ok(result, "ast_dump")) {
    return false;
  }

  const std::string dump = glsl2llvm::ast::dump(*result.unit);
  if (dump.find("TranslationUnit") == std::string::npos) {
    return fail("ast_dump: missing TranslationUnit");
  }
  if (dump.find("FunctionDecl") == std::string::npos) {
    return fail("ast_dump: missing FunctionDecl");
  }
  if (dump.find("VarDecl") == std::string::npos) {
    return fail("ast_dump: missing VarDecl");
  }
  if (dump.find("ReturnStmt") == std::string::npos) {
    return fail("ast_dump: missing ReturnStmt");
  }
  if (dump.find("BinaryExpr") == std::string::npos) {
    return fail("ast_dump: missing BinaryExpr");
  }

  return true;
}

bool case_parse_error_missing_semicolon() {
  const ParseResult result = parse_source("void main() { float a = 1.0 }");
  if (result.errors.empty()) {
    return fail("parse_error_missing_semicolon: expected parser errors");
  }

  for (const auto &error : result.errors) {
    if (error.message.find("expected ';'") != std::string::npos) {
      return true;
    }
  }

  return fail("parse_error_missing_semicolon: expected semicolon-related error");
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[1]) != "--case") {
    std::cerr << "usage: parser_test --case <name>\n";
    return 2;
  }

  const std::unordered_map<std::string, std::function<bool()>> cases = {
      {"function_basic", case_function_basic},
      {"function_with_return_binary", case_function_with_return_binary},
      {"two_functions", case_two_functions},
      {"local_var_init", case_local_var_init},
      {"local_var_no_init", case_local_var_no_init},
      {"nested_block", case_nested_block},
      {"if_stmt", case_if_stmt},
      {"for_stmt_var_init", case_for_stmt_var_init},
      {"for_stmt_expr_init", case_for_stmt_expr_init},
      {"call_expr", case_call_expr},
      {"member_expr", case_member_expr},
      {"precedence_mul_add", case_precedence_mul_add},
      {"precedence_paren", case_precedence_paren},
      {"ast_dump", case_ast_dump},
      {"parse_error_missing_semicolon", case_parse_error_missing_semicolon},
  };

  const std::string case_name = argv[2];
  const auto it = cases.find(case_name);
  if (it == cases.end()) {
    std::cerr << "unknown case: " << case_name << "\n";
    return 2;
  }

  return it->second() ? 0 : 1;
}
