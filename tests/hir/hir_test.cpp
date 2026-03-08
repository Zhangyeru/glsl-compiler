#include "hir/hir.h"
#include "hir/lowering.h"
#include "parser/parser.h"
#include "sema/semantic_analyzer.h"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {

using glsl2llvm::hir::ASTLowerer;
using glsl2llvm::hir::Call;
using glsl2llvm::hir::Cast;
using glsl2llvm::hir::LoweringResult;
using glsl2llvm::hir::Node;
using glsl2llvm::hir::NodeKind;
using glsl2llvm::hir::Return;
using glsl2llvm::hir::VarDecl;
using glsl2llvm::parser::ParseResult;
using glsl2llvm::parser::Parser;
using glsl2llvm::sema::SemanticAnalyzer;
using glsl2llvm::sema::SemaResult;
using glsl2llvm::sema::TypeKind;

struct PipelineOutput {
  ParseResult parse;
  SemaResult sema;
  LoweringResult hir;
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

PipelineOutput run_pipeline(const std::string &source) {
  Parser parser(source);
  PipelineOutput output{parser.parse_translation_unit(), SemaResult{}, LoweringResult{}};

  if (output.parse.unit != nullptr) {
    SemanticAnalyzer analyzer;
    output.sema = analyzer.analyze(*output.parse.unit);

    ASTLowerer lowerer;
    output.hir = lowerer.lower(*output.parse.unit);
  }

  return output;
}

template <typename T>
const T *as(const Node *node) {
  return dynamic_cast<const T *>(node);
}

bool case_hir_basic_binary() {
  const PipelineOutput output = run_pipeline("float foo(float x) { return x * 2.0; }");
  if (!output.parse.ok()) {
    return fail("hir_basic_binary: parser errors:\n" + format_parse_errors(output.parse));
  }
  if (!output.sema.ok()) {
    return fail("hir_basic_binary: sema should be clean");
  }
  if (output.hir.module == nullptr || output.hir.module->functions.size() != 1) {
    return fail("hir_basic_binary: expected one lowered function");
  }

  const auto &fn = *output.hir.module->functions[0];
  if (fn.entry == nullptr || fn.entry->statements.empty()) {
    return fail("hir_basic_binary: missing function body");
  }

  const auto *ret = as<Return>(fn.entry->statements[0].get());
  if (ret == nullptr || ret->value == nullptr) {
    return fail("hir_basic_binary: missing return value");
  }

  if (ret->value->kind != NodeKind::Binary) {
    return fail("hir_basic_binary: expected HIRBinary return value");
  }

  return true;
}

bool case_hir_builtin_load() {
  const PipelineOutput output = run_pipeline("uint foo() { return gl_GlobalInvocationID.x; }");
  if (!output.parse.ok()) {
    return fail("hir_builtin_load: parser errors\n" + format_parse_errors(output.parse));
  }

  if (output.hir.module == nullptr || output.hir.module->functions.empty()) {
    return fail("hir_builtin_load: missing HIR module/function");
  }

  const auto *ret = as<Return>(output.hir.module->functions[0]->entry->statements[0].get());
  if (ret == nullptr || ret->value == nullptr) {
    return fail("hir_builtin_load: missing return value");
  }

  if (ret->value->kind != NodeKind::BuiltinLoad) {
    return fail("hir_builtin_load: expected HIRBuiltinLoad");
  }

  const auto *load = static_cast<const glsl2llvm::hir::BuiltinLoad *>(ret->value.get());
  if (load->builtin_name != "gl_GlobalInvocationID" || load->component != "x") {
    return fail("hir_builtin_load: wrong builtin lowering");
  }

  return load->type == TypeKind::Uint;
}

bool case_hir_implicit_cast() {
  const PipelineOutput output = run_pipeline("void main() { float a = 1; }");
  if (!output.parse.ok()) {
    return fail("hir_implicit_cast: parser errors\n" + format_parse_errors(output.parse));
  }

  if (output.hir.module == nullptr || output.hir.module->functions.empty()) {
    return fail("hir_implicit_cast: missing lowered function");
  }

  const auto *decl = as<VarDecl>(output.hir.module->functions[0]->entry->statements[0].get());
  if (decl == nullptr || decl->initializer == nullptr) {
    return fail("hir_implicit_cast: missing var initializer");
  }

  if (decl->initializer->kind != NodeKind::Cast) {
    return fail("hir_implicit_cast: expected explicit HIRCast");
  }

  const auto *cast = static_cast<const Cast *>(decl->initializer.get());
  return cast->to_type == TypeKind::Float;
}

bool case_hir_vector_constructor() {
  const PipelineOutput output = run_pipeline("vec3 makev(int a) { return vec3(a, 2, 3.0); }");
  if (!output.parse.ok()) {
    return fail("hir_vector_constructor: parser errors\n" + format_parse_errors(output.parse));
  }

  if (output.hir.module == nullptr || output.hir.module->functions.empty()) {
    return fail("hir_vector_constructor: missing lowered function");
  }

  const auto *ret = as<Return>(output.hir.module->functions[0]->entry->statements[0].get());
  if (ret == nullptr || ret->value == nullptr) {
    return fail("hir_vector_constructor: missing return value");
  }

  const auto *call = as<Call>(ret->value.get());
  if (call == nullptr) {
    return fail("hir_vector_constructor: expected call node");
  }

  if (call->callee != "__ctor.vec3") {
    return fail("hir_vector_constructor: expected vec3 constructor lowering");
  }

  if (call->args.size() != 3) {
    return fail("hir_vector_constructor: expected 3 constructor args");
  }

  if (call->args[0]->kind != NodeKind::Cast || call->args[1]->kind != NodeKind::Cast) {
    return fail("hir_vector_constructor: expected casts on integer args");
  }

  return true;
}

bool case_hir_dump_control_flow() {
  const PipelineOutput output = run_pipeline(
      "void main() { int i = 0; for (i = 0; i; i = i - 1) { if (i) { return; } } }");
  if (!output.parse.ok()) {
    return fail("hir_dump_control_flow: parser errors\n" + format_parse_errors(output.parse));
  }

  if (output.hir.module == nullptr) {
    return fail("hir_dump_control_flow: missing module");
  }

  const std::string text = glsl2llvm::hir::dump(*output.hir.module);
  if (text.find("HIRLoop") == std::string::npos) {
    return fail("hir_dump_control_flow: dump missing HIRLoop");
  }
  if (text.find("HIRIf") == std::string::npos) {
    return fail("hir_dump_control_flow: dump missing HIRIf");
  }
  if (text.find("HIRCast") == std::string::npos) {
    return fail("hir_dump_control_flow: dump missing HIRCast");
  }

  return true;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[1]) != "--case") {
    std::cerr << "usage: hir_test --case <name>\n";
    return 2;
  }

  const std::unordered_map<std::string, std::function<bool()>> cases = {
      {"hir_basic_binary", case_hir_basic_binary},
      {"hir_builtin_load", case_hir_builtin_load},
      {"hir_implicit_cast", case_hir_implicit_cast},
      {"hir_vector_constructor", case_hir_vector_constructor},
      {"hir_dump_control_flow", case_hir_dump_control_flow},
  };

  const std::string case_name = argv[2];
  const auto it = cases.find(case_name);
  if (it == cases.end()) {
    std::cerr << "unknown case: " << case_name << "\n";
    return 2;
  }

  return it->second() ? 0 : 1;
}
