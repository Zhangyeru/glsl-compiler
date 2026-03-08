#include "codegen/hir_codegen.h"
#include "hir/lowering.h"
#include "parser/parser.h"
#include "sema/semantic_analyzer.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

struct CliOptions {
  bool help = false;
  bool emit_llvm_ir = false;
  std::string input_path;
  std::string output_path;
};

void print_help(std::ostream &os, const char *argv0) {
  os << "Usage: " << argv0 << " input.comp -S -o out.ll\n"
     << "\n"
     << "Options:\n"
     << "  -S         Emit LLVM IR text output (.ll).\n"
     << "  -o <file>  Output path.\n"
     << "  -h, --help Show this help message.\n";
}

bool parse_cli(int argc, char **argv, CliOptions &options, std::string &error) {
  if (argc <= 1) {
    options.help = true;
    return true;
  }

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
      options.help = true;
      return true;
    }

    if (arg == "-S") {
      options.emit_llvm_ir = true;
      continue;
    }

    if (arg == "-o") {
      if (i + 1 >= argc) {
        error = "missing output file after -o";
        return false;
      }
      options.output_path = argv[++i];
      continue;
    }

    if (!arg.empty() && arg[0] == '-') {
      error = "unknown option: " + arg;
      return false;
    }

    if (!options.input_path.empty()) {
      error = "multiple input files are not supported";
      return false;
    }
    options.input_path = arg;
  }

  if (options.input_path.empty()) {
    error = "missing input file";
    return false;
  }

  if (!options.emit_llvm_ir) {
    error = "-S is required in this prototype";
    return false;
  }

  if (options.output_path.empty()) {
    options.output_path = "a.ll";
  }

  return true;
}

}  // namespace

int main(int argc, char **argv) {
  CliOptions options;
  std::string error;

  if (!parse_cli(argc, argv, options, error)) {
    std::cerr << "error: " << error << "\n\n";
    print_help(std::cerr, argv[0]);
    return 1;
  }

  if (options.help) {
    print_help(std::cout, argv[0]);
    return 0;
  }

  std::ifstream input(options.input_path, std::ios::binary);
  if (!input) {
    std::cerr << "error: unable to open input file: " << options.input_path << "\n";
    return 1;
  }

  const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

  glsl2llvm::parser::Parser parser(source, options.input_path);
  glsl2llvm::parser::ParseResult parse_result = parser.parse_translation_unit();
  if (!parse_result.ok() || parse_result.unit == nullptr) {
    for (const auto &error : parse_result.errors) {
      std::cerr << options.input_path << ":" << error.location.line << ":" << error.location.column
                << ": parser error: " << error.message << "\n";
    }
    return 1;
  }

  glsl2llvm::sema::SemanticAnalyzer sema;
  glsl2llvm::sema::SemaResult sema_result = sema.analyze(*parse_result.unit);
  if (!sema_result.ok()) {
    for (const auto &error : sema_result.errors) {
      std::cerr << options.input_path << ":" << error.location.line << ":" << error.location.column
                << ": semantic error: " << error.message << "\n";
    }
    return 1;
  }

  glsl2llvm::hir::ASTLowerer lowerer;
  glsl2llvm::hir::LoweringResult lowering_result = lowerer.lower(*parse_result.unit);
  if (!lowering_result.ok() || lowering_result.module == nullptr) {
    for (const auto &error : lowering_result.errors) {
      std::cerr << options.input_path << ":" << error.location.line << ":" << error.location.column
                << ": lowering error: " << error.message << "\n";
    }
    return 1;
  }

  glsl2llvm::codegen::HIRCodeGenerator codegen;
  glsl2llvm::codegen::LLVMCodegenResult codegen_result =
      codegen.generate(*lowering_result.module, options.input_path);
  if (!codegen_result.ok() || codegen_result.module == nullptr) {
    for (const auto &error : codegen_result.errors) {
      std::cerr << "codegen error: " << error << "\n";
    }
    return 1;
  }

  std::ofstream out(options.output_path, std::ios::binary);
  if (!out) {
    std::cerr << "error: unable to open output file: " << options.output_path << "\n";
    return 1;
  }

  out << glsl2llvm::codegen::module_to_string(*codegen_result.module);
  out.flush();
  if (!out) {
    std::cerr << "error: failed to write LLVM IR to output file: " << options.output_path << "\n";
    return 1;
  }

  return 0;
}
