#include "ast/ast.h"
#include "parser/parser.h"
#include "sema/semantic_analyzer.h"
#include "sema/type.h"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

using glsl2llvm::ast::BinaryExpr;
using glsl2llvm::ast::FunctionDecl;
using glsl2llvm::ast::LiteralExpr;
using glsl2llvm::ast::ReturnStmt;
using glsl2llvm::ast::VarDecl;
using glsl2llvm::parser::ParseResult;
using glsl2llvm::parser::Parser;
using glsl2llvm::sema::SemanticAnalyzer;
using glsl2llvm::sema::SemaResult;
using glsl2llvm::sema::TypeKind;

struct AnalysisOutput {
  ParseResult parse;
  SemaResult sema;
};

bool fail(const std::string &message) {
  std::cerr << message << "\n";
  return false;
}

std::string format_parse_errors(const ParseResult &result) {
  std::string out;
  for (const auto &error : result.errors) {
    out += std::to_string(error.location.line) + ":" + std::to_string(error.location.column) +
           " " + error.message + "\n";
  }
  return out;
}

std::string format_sema_errors(const SemaResult &result) {
  std::string out;
  for (const auto &error : result.errors) {
    out += std::to_string(error.location.line) + ":" + std::to_string(error.location.column) +
           " " + error.message + "\n";
  }
  return out;
}

AnalysisOutput run_analysis(const std::string &source) {
  Parser parser(source);
  AnalysisOutput output{parser.parse_translation_unit(), SemaResult{}};

  if (output.parse.unit != nullptr) {
    SemanticAnalyzer analyzer;
    output.sema = analyzer.analyze(*output.parse.unit);
  }

  return output;
}

bool contains_sema_error(const SemaResult &result, const std::string &needle) {
  for (const auto &error : result.errors) {
    if (error.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

template <typename T>
const T *as(const glsl2llvm::ast::Node *node) {
  return dynamic_cast<const T *>(node);
}

bool case_sema_ok_basic() {
  const AnalysisOutput output = run_analysis(
      "float foo(float x) {\n"
      "  return x * 2.0;\n"
      "}\n"
      "void main() {\n"
      "  float a = 1.0;\n"
      "}\n");

  if (!output.parse.ok()) {
    return fail("sema_ok_basic: parser errors:\n" + format_parse_errors(output.parse));
  }
  if (!output.sema.ok()) {
    return fail("sema_ok_basic: sema errors:\n" + format_sema_errors(output.sema));
  }
  return true;
}

bool case_sema_undefined_variable() {
  const AnalysisOutput output = run_analysis("float foo() { return x; }");
  if (!output.parse.ok()) {
    return fail("sema_undefined_variable: parser errors:\n" + format_parse_errors(output.parse));
  }

  if (!contains_sema_error(output.sema, "undefined variable")) {
    return fail("sema_undefined_variable: expected undefined variable error");
  }
  return true;
}

bool case_sema_duplicate_definition() {
  const AnalysisOutput output = run_analysis("void main() { float a = 1.0; float a = 2.0; }");
  if (!output.parse.ok()) {
    return fail("sema_duplicate_definition: parser errors:\n" + format_parse_errors(output.parse));
  }

  if (!contains_sema_error(output.sema, "duplicate variable definition")) {
    return fail("sema_duplicate_definition: expected duplicate definition error");
  }
  return true;
}

bool case_sema_type_mismatch_initializer() {
  const AnalysisOutput output = run_analysis("void main() { int a = 1.0; }");
  if (!output.parse.ok()) {
    return fail("sema_type_mismatch_initializer: parser errors:\n" + format_parse_errors(output.parse));
  }

  if (!contains_sema_error(output.sema, "type mismatch in initializer")) {
    return fail("sema_type_mismatch_initializer: expected initializer type mismatch error");
  }
  return true;
}

bool case_sema_function_arg_count() {
  const AnalysisOutput output = run_analysis(
      "float foo(float x) { return x; }\n"
      "void main() { float a = foo(); }\n");
  if (!output.parse.ok()) {
    return fail("sema_function_arg_count: parser errors:\n" + format_parse_errors(output.parse));
  }

  if (!contains_sema_error(output.sema, "argument count mismatch")) {
    return fail("sema_function_arg_count: expected argument count mismatch error");
  }
  return true;
}

bool case_sema_function_arg_type() {
  const AnalysisOutput output = run_analysis(
      "float foo(float x) { return x; }\n"
      "void main() { float a = foo(1); }\n");
  if (!output.parse.ok()) {
    return fail("sema_function_arg_type: parser errors:\n" + format_parse_errors(output.parse));
  }

  if (!contains_sema_error(output.sema, "argument type mismatch")) {
    return fail("sema_function_arg_type: expected argument type mismatch error");
  }
  return true;
}

bool case_sema_builtin_member_ok() {
  const AnalysisOutput output = run_analysis("uint foo() { return gl_GlobalInvocationID.x; }");
  if (!output.parse.ok()) {
    return fail("sema_builtin_member_ok: parser errors:\n" + format_parse_errors(output.parse));
  }
  if (!output.sema.ok()) {
    return fail("sema_builtin_member_ok: sema errors:\n" + format_sema_errors(output.sema));
  }
  return true;
}

bool case_sema_builtin_member_invalid() {
  const AnalysisOutput output = run_analysis("float foo() { return gl_GlobalInvocationID.q; }");
  if (!output.parse.ok()) {
    return fail("sema_builtin_member_invalid: parser errors:\n" + format_parse_errors(output.parse));
  }

  if (!contains_sema_error(output.sema, "invalid member access")) {
    return fail("sema_builtin_member_invalid: expected invalid member error");
  }
  return true;
}

bool case_sema_return_type_mismatch() {
  const AnalysisOutput output = run_analysis("int foo() { return 1.0; }");
  if (!output.parse.ok()) {
    return fail("sema_return_type_mismatch: parser errors:\n" + format_parse_errors(output.parse));
  }

  if (!contains_sema_error(output.sema, "return type mismatch")) {
    return fail("sema_return_type_mismatch: expected return type mismatch error");
  }
  return true;
}

bool case_sema_resolved_type_attached() {
  const AnalysisOutput output = run_analysis(
      "float foo(float x) { return x * 2.0; }\n"
      "void main() { float a = 1.0; }\n");
  if (!output.parse.ok()) {
    return fail("sema_resolved_type_attached: parser errors:\n" + format_parse_errors(output.parse));
  }
  if (!output.sema.ok()) {
    return fail("sema_resolved_type_attached: sema errors:\n" + format_sema_errors(output.sema));
  }

  if (output.parse.unit->resolved_type == TypeKind::Unresolved) {
    return fail("sema_resolved_type_attached: translation unit unresolved");
  }

  const FunctionDecl &foo = *output.parse.unit->functions[0];
  if (foo.resolved_type != TypeKind::Float) {
    return fail("sema_resolved_type_attached: function resolved type mismatch");
  }

  const VarDecl &param = *foo.parameters[0];
  if (param.resolved_type != TypeKind::Float) {
    return fail("sema_resolved_type_attached: parameter unresolved");
  }

  const auto *ret = as<ReturnStmt>(foo.body->statements[0].get());
  if (ret == nullptr || ret->resolved_type != TypeKind::Float) {
    return fail("sema_resolved_type_attached: return stmt unresolved");
  }

  const auto *binary = as<BinaryExpr>(ret->value.get());
  if (binary == nullptr || binary->resolved_type != TypeKind::Float) {
    return fail("sema_resolved_type_attached: binary expr unresolved");
  }

  const FunctionDecl &main_fn = *output.parse.unit->functions[1];
  const auto *decl = as<VarDecl>(main_fn.body->statements[0].get());
  if (decl == nullptr || decl->resolved_type != TypeKind::Float) {
    return fail("sema_resolved_type_attached: var decl unresolved");
  }

  const auto *init = as<LiteralExpr>(decl->initializer.get());
  if (init == nullptr || init->resolved_type != TypeKind::Float) {
    return fail("sema_resolved_type_attached: literal expr unresolved");
  }

  return true;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[1]) != "--case") {
    std::cerr << "usage: sema_test --case <name>\n";
    return 2;
  }

  const std::unordered_map<std::string, std::function<bool()>> cases = {
      {"sema_ok_basic", case_sema_ok_basic},
      {"sema_undefined_variable", case_sema_undefined_variable},
      {"sema_duplicate_definition", case_sema_duplicate_definition},
      {"sema_type_mismatch_initializer", case_sema_type_mismatch_initializer},
      {"sema_function_arg_count", case_sema_function_arg_count},
      {"sema_function_arg_type", case_sema_function_arg_type},
      {"sema_builtin_member_ok", case_sema_builtin_member_ok},
      {"sema_builtin_member_invalid", case_sema_builtin_member_invalid},
      {"sema_return_type_mismatch", case_sema_return_type_mismatch},
      {"sema_resolved_type_attached", case_sema_resolved_type_attached},
  };

  const std::string case_name = argv[2];
  const auto it = cases.find(case_name);
  if (it == cases.end()) {
    std::cerr << "unknown case: " << case_name << "\n";
    return 2;
  }

  return it->second() ? 0 : 1;
}
