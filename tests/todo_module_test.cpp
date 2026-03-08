#include "codegen/todo_module.h"

#include <iostream>
#include <string>

int main() {
  const std::string a = glsl2llvm::codegen::sanitize_module_name("examples/minimal.comp");
  if (a != "minimal_comp") {
    std::cerr << "unexpected module name for path: " << a << "\n";
    return 1;
  }

  const std::string b = glsl2llvm::codegen::sanitize_module_name("123 name.comp");
  if (b != "_123_name_comp") {
    std::cerr << "unexpected module name for invalid chars: " << b << "\n";
    return 1;
  }

  const std::string c = glsl2llvm::codegen::sanitize_module_name("");
  if (c != "module") {
    std::cerr << "unexpected module name for empty path: " << c << "\n";
    return 1;
  }

  return 0;
}
