#pragma once

#include <string>
#include <string_view>

namespace glsl2llvm::codegen {

struct TodoModuleOptions {
  std::string input_path;
  std::string output_path;
  std::string module_name;
};

struct TodoModuleResult {
  bool ok = false;
  std::string message;
};

std::string sanitize_module_name(std::string_view input_path);
TodoModuleResult emit_todo_module(const TodoModuleOptions &options);

}  // namespace glsl2llvm::codegen
