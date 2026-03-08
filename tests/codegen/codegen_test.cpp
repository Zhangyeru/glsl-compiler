#include "codegen/hir_codegen.h"
#include "hir/hir.h"
#include "hir/lowering.h"
#include "parser/parser.h"
#include "sema/semantic_analyzer.h"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {

using glsl2llvm::codegen::HIRCodeGenerator;
using glsl2llvm::codegen::LLVMCodegenResult;
using glsl2llvm::codegen::module_to_string;
using glsl2llvm::hir::ASTLowerer;
using glsl2llvm::hir::Call;
using glsl2llvm::hir::ExprStmt;
using glsl2llvm::hir::LoweringResult;
using glsl2llvm::hir::Loop;
using glsl2llvm::hir::Module;
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
  std::string text;
  for (const auto &error : result.errors) {
    text += std::to_string(error.location.line) + ":" + std::to_string(error.location.column) +
            " " + error.message + "\n";
  }
  return text;
}

std::string format_sema_errors(const SemaResult &result) {
  std::string text;
  for (const auto &error : result.errors) {
    text += std::to_string(error.location.line) + ":" + std::to_string(error.location.column) +
            " " + error.message + "\n";
  }
  return text;
}

std::string format_hir_errors(const LoweringResult &result) {
  std::string text;
  for (const auto &error : result.errors) {
    text += std::to_string(error.location.line) + ":" + std::to_string(error.location.column) +
            " " + error.message + "\n";
  }
  return text;
}

std::string format_codegen_errors(const LLVMCodegenResult &result) {
  std::string text;
  for (const auto &error : result.errors) {
    text += error + "\n";
  }
  return text;
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

LLVMCodegenResult codegen_from_hir(const Module &hir_module) {
  HIRCodeGenerator generator;
  return generator.generate(hir_module, "test.module");
}

bool case_ir_binary_ops() {
  const PipelineOutput output = run_pipeline("float foo(float x) { return x * 2.0 + x / 2.0; }");
  if (!output.parse.ok()) {
    return fail("ir_binary_ops: parser errors:\n" + format_parse_errors(output.parse));
  }
  if (!output.sema.ok()) {
    return fail("ir_binary_ops: sema errors:\n" + format_sema_errors(output.sema));
  }
  if (!output.hir.ok() || output.hir.module == nullptr) {
    return fail("ir_binary_ops: hir lowering errors:\n" + format_hir_errors(output.hir));
  }

  const LLVMCodegenResult result = codegen_from_hir(*output.hir.module);
  if (!result.ok()) {
    return fail("ir_binary_ops: codegen errors:\n" + format_codegen_errors(result));
  }

  const std::string ir = module_to_string(*result.module);
  if (ir.find("fmul") == std::string::npos) {
    return fail("ir_binary_ops: missing fmul");
  }
  if (ir.find("fdiv") == std::string::npos) {
    return fail("ir_binary_ops: missing fdiv");
  }
  if (ir.find("fadd") == std::string::npos) {
    return fail("ir_binary_ops: missing fadd");
  }
  if (ir.find("global_id_x") == std::string::npos || ir.find("data") == std::string::npos) {
    return fail("ir_binary_ops: missing required entry params");
  }

  return true;
}

bool case_ir_builtin_global_id_x() {
  const PipelineOutput output = run_pipeline("uint foo() { return gl_GlobalInvocationID.x; }");
  if (!output.parse.ok()) {
    return fail("ir_builtin_global_id_x: parser errors:\n" + format_parse_errors(output.parse));
  }
  if (!output.sema.ok()) {
    return fail("ir_builtin_global_id_x: sema errors:\n" + format_sema_errors(output.sema));
  }
  if (!output.hir.ok() || output.hir.module == nullptr) {
    return fail("ir_builtin_global_id_x: hir lowering errors:\n" + format_hir_errors(output.hir));
  }

  const LLVMCodegenResult result = codegen_from_hir(*output.hir.module);
  if (!result.ok()) {
    return fail("ir_builtin_global_id_x: codegen errors:\n" + format_codegen_errors(result));
  }

  const std::string ir = module_to_string(*result.module);
  if (ir.find("define i32 @foo") == std::string::npos) {
    return fail("ir_builtin_global_id_x: function signature should return i32");
  }
  if (ir.find("ret i32 %global_id_x") == std::string::npos) {
    return fail("ir_builtin_global_id_x: missing direct global id return");
  }

  return true;
}

bool case_ir_if_for_control_flow() {
  auto module = std::make_unique<Module>();
  auto fn = std::make_unique<glsl2llvm::hir::Function>("main", TypeKind::Void);
  fn->entry = std::make_unique<glsl2llvm::hir::Block>();

  auto decl = std::make_unique<VarDecl>("i", TypeKind::Int);
  decl->initializer = std::make_unique<glsl2llvm::hir::Literal>("2", TypeKind::Int);
  fn->entry->statements.push_back(std::move(decl));

  auto loop = std::make_unique<Loop>();

  auto init_rhs = std::make_unique<glsl2llvm::hir::Literal>("0", TypeKind::Int);
  loop->init = std::make_unique<ExprStmt>(std::make_unique<glsl2llvm::hir::Binary>(
      "=", std::make_unique<glsl2llvm::hir::Variable>("i", TypeKind::Int), std::move(init_rhs),
      TypeKind::Int));

  loop->condition = std::make_unique<glsl2llvm::hir::Cast>(
      std::make_unique<glsl2llvm::hir::Variable>("i", TypeKind::Int), TypeKind::Int,
      TypeKind::Bool);

  auto dec_expr = std::make_unique<glsl2llvm::hir::Binary>(
      "-", std::make_unique<glsl2llvm::hir::Variable>("i", TypeKind::Int),
      std::make_unique<glsl2llvm::hir::Literal>("1", TypeKind::Int), TypeKind::Int);
  loop->update = std::make_unique<glsl2llvm::hir::Binary>(
      "=", std::make_unique<glsl2llvm::hir::Variable>("i", TypeKind::Int), std::move(dec_expr),
      TypeKind::Int);

  loop->body = std::make_unique<glsl2llvm::hir::Block>();
  auto if_stmt = std::make_unique<glsl2llvm::hir::If>();
  if_stmt->condition = std::make_unique<glsl2llvm::hir::Cast>(
      std::make_unique<glsl2llvm::hir::Variable>("i", TypeKind::Int), TypeKind::Int,
      TypeKind::Bool);
  if_stmt->then_block = std::make_unique<glsl2llvm::hir::Block>();
  if_stmt->then_block->statements.push_back(std::make_unique<Return>());
  loop->body->statements.push_back(std::move(if_stmt));

  fn->entry->statements.push_back(std::move(loop));
  fn->entry->statements.push_back(std::make_unique<Return>());

  module->functions.push_back(std::move(fn));

  const LLVMCodegenResult result = codegen_from_hir(*module);
  if (!result.ok()) {
    return fail("ir_if_for_control_flow: codegen errors:\n" + format_codegen_errors(result));
  }

  const std::string ir = module_to_string(*result.module);
  if (ir.find("loop.cond") == std::string::npos) {
    return fail("ir_if_for_control_flow: missing loop cond block");
  }
  if (ir.find("if.then") == std::string::npos) {
    return fail("ir_if_for_control_flow: missing if.then block");
  }
  if (ir.find("br i1") == std::string::npos) {
    return fail("ir_if_for_control_flow: missing conditional branch");
  }

  return true;
}

bool case_ir_buffer_load_store() {
  auto module = std::make_unique<Module>();
  auto fn = std::make_unique<glsl2llvm::hir::Function>("foo", TypeKind::Float);
  fn->entry = std::make_unique<glsl2llvm::hir::Block>();

  auto store = std::make_unique<glsl2llvm::hir::BufferStore>("data", "");
  store->index = std::make_unique<glsl2llvm::hir::Literal>("0", TypeKind::Int);
  store->value = std::make_unique<glsl2llvm::hir::Literal>("1.25", TypeKind::Float);
  fn->entry->statements.push_back(std::make_unique<ExprStmt>(std::move(store)));

  auto load = std::make_unique<glsl2llvm::hir::BufferLoad>("data", "", TypeKind::Float);
  load->index = std::make_unique<glsl2llvm::hir::Literal>("0", TypeKind::Int);

  auto ret = std::make_unique<Return>();
  ret->value = std::move(load);
  fn->entry->statements.push_back(std::move(ret));

  module->functions.push_back(std::move(fn));

  const LLVMCodegenResult result = codegen_from_hir(*module);
  if (!result.ok()) {
    return fail("ir_buffer_load_store: codegen errors:\n" + format_codegen_errors(result));
  }

  const std::string ir = module_to_string(*result.module);
  if (ir.find("store float") == std::string::npos) {
    return fail("ir_buffer_load_store: missing store");
  }
  if (ir.find("load float") == std::string::npos) {
    return fail("ir_buffer_load_store: missing load");
  }

  return true;
}

bool case_ir_vec_type_mapping() {
  auto module = std::make_unique<Module>();
  auto fn = std::make_unique<glsl2llvm::hir::Function>("makev", TypeKind::Vec3);
  fn->entry = std::make_unique<glsl2llvm::hir::Block>();

  auto ctor = std::make_unique<Call>("__ctor.vec3", TypeKind::Vec3);
  ctor->args.push_back(std::make_unique<glsl2llvm::hir::Literal>("1.0", TypeKind::Float));
  ctor->args.push_back(std::make_unique<glsl2llvm::hir::Literal>("2", TypeKind::Int));
  ctor->args.push_back(std::make_unique<glsl2llvm::hir::Literal>("3.0", TypeKind::Float));

  auto ret = std::make_unique<Return>();
  ret->value = std::move(ctor);
  fn->entry->statements.push_back(std::move(ret));
  module->functions.push_back(std::move(fn));

  const LLVMCodegenResult result = codegen_from_hir(*module);
  if (!result.ok()) {
    return fail("ir_vec_type_mapping: codegen errors:\n" + format_codegen_errors(result));
  }

  const std::string ir = module_to_string(*result.module);
  if (ir.find("define <3 x float>") == std::string::npos) {
    return fail("ir_vec_type_mapping: wrong vec3 function mapping");
  }
  if (ir.find("insertelement") == std::string::npos &&
      ir.find("ret <3 x float>") == std::string::npos) {
    return fail("ir_vec_type_mapping: missing vector construction/return");
  }

  return true;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[1]) != "--case") {
    std::cerr << "usage: codegen_test --case <name>\n";
    return 2;
  }

  const std::unordered_map<std::string, std::function<bool()>> cases = {
      {"ir_binary_ops", case_ir_binary_ops},
      {"ir_builtin_global_id_x", case_ir_builtin_global_id_x},
      {"ir_if_for_control_flow", case_ir_if_for_control_flow},
      {"ir_buffer_load_store", case_ir_buffer_load_store},
      {"ir_vec_type_mapping", case_ir_vec_type_mapping},
  };

  const std::string case_name = argv[2];
  const auto it = cases.find(case_name);
  if (it == cases.end()) {
    std::cerr << "unknown case: " << case_name << "\n";
    return 2;
  }

  return it->second() ? 0 : 1;
}
