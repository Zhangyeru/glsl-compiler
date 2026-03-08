#pragma once

#include "hir/hir.h"

#if defined(GLSL2LLVM_HAVE_LLVM_CONFIG)
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#else
namespace llvm {
class LLVMContext;
class Module;
}  // namespace llvm
#endif

#include <memory>
#include <string>
#include <vector>

namespace glsl2llvm::codegen {

struct LLVMContextDeleter {
#if defined(GLSL2LLVM_HAVE_LLVM_CONFIG)
  void operator()(llvm::LLVMContext *ptr) const noexcept { delete ptr; }
#else
  void operator()(llvm::LLVMContext *ptr) const noexcept { (void)ptr; }
#endif
};

struct LLVMModuleDeleter {
#if defined(GLSL2LLVM_HAVE_LLVM_CONFIG)
  void operator()(llvm::Module *ptr) const noexcept { delete ptr; }
#else
  void operator()(llvm::Module *ptr) const noexcept { (void)ptr; }
#endif
};

struct LLVMCodegenResult {
  std::unique_ptr<llvm::LLVMContext, LLVMContextDeleter> context;
  std::unique_ptr<llvm::Module, LLVMModuleDeleter> module;
  std::vector<std::string> errors;

  [[nodiscard]] bool ok() const { return module != nullptr && errors.empty(); }
};

class HIRCodeGenerator {
 public:
  LLVMCodegenResult generate(const hir::Module &hir_module, std::string module_name = "glsl2llvm.module");
};

std::string module_to_string(const llvm::Module &module);

}  // namespace glsl2llvm::codegen
