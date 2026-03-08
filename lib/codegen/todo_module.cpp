#include "codegen/todo_module.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <system_error>

#if defined(GLSL2LLVM_HAVE_LLVM_CONFIG)
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#else
extern "C" {
typedef struct LLVMOpaqueContext *LLVMContextRef;
typedef struct LLVMOpaqueModule *LLVMModuleRef;

LLVMContextRef LLVMContextCreate(void);
void LLVMContextDispose(LLVMContextRef C);
LLVMModuleRef LLVMModuleCreateWithNameInContext(const char *ModuleID, LLVMContextRef C);
void LLVMSetSourceFileName(LLVMModuleRef M, const char *Name, std::size_t Len);
int LLVMPrintModuleToFile(LLVMModuleRef M, const char *Filename, char **ErrorMessage);
void LLVMDisposeMessage(char *Message);
void LLVMDisposeModule(LLVMModuleRef M);
}
#endif

namespace glsl2llvm::codegen {

std::string sanitize_module_name(std::string_view input_path) {
  std::string base(input_path);
  const std::size_t slash = base.find_last_of("/\\");
  if (slash != std::string::npos) {
    base = base.substr(slash + 1);
  }

  if (base.empty()) {
    base = "module";
  }

  for (char &ch : base) {
    const bool valid = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
    if (!valid) {
      ch = '_';
    }
  }

  if (!base.empty() && std::isdigit(static_cast<unsigned char>(base.front()))) {
    base.insert(base.begin(), '_');
  }

  if (base.empty()) {
    return "module";
  }

  return base;
}

TodoModuleResult emit_todo_module(const TodoModuleOptions &options) {
  if (options.input_path.empty()) {
    return {false, "input path is empty"};
  }
  if (options.output_path.empty()) {
    return {false, "output path is empty"};
  }

  const std::string module_name =
      options.module_name.empty() ? sanitize_module_name(options.input_path) : options.module_name;

#if defined(GLSL2LLVM_HAVE_LLVM_CONFIG)
  llvm::LLVMContext context;
  llvm::Module module(module_name, context);
  module.setSourceFileName(options.input_path);

  std::error_code ec;
  llvm::raw_fd_ostream out(options.output_path, ec, llvm::sys::fs::OF_Text);
  if (ec) {
    return {false, "unable to open output file: " + ec.message()};
  }

  module.print(out, nullptr);
  out.flush();
  return {true, "placeholder LLVM IR emitted"};
#else
  LLVMContextRef context = LLVMContextCreate();
  if (context == nullptr) {
    return {false, "LLVMContextCreate failed"};
  }

  LLVMModuleRef module = LLVMModuleCreateWithNameInContext(module_name.c_str(), context);
  if (module == nullptr) {
    LLVMContextDispose(context);
    return {false, "LLVMModuleCreateWithNameInContext failed"};
  }

  LLVMSetSourceFileName(module, options.input_path.c_str(), options.input_path.size());

  char *error_message = nullptr;
  const int rc = LLVMPrintModuleToFile(module, options.output_path.c_str(), &error_message);

  LLVMDisposeModule(module);
  LLVMContextDispose(context);

  if (rc != 0) {
    std::string msg = "LLVMPrintModuleToFile failed";
    if (error_message != nullptr) {
      msg += ": ";
      msg += error_message;
      LLVMDisposeMessage(error_message);
    }
    return {false, msg};
  }

  return {true, "placeholder LLVM IR emitted"};
#endif
}

}  // namespace glsl2llvm::codegen
