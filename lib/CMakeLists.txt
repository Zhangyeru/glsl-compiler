add_library(glsl2llvm_frontend
  codegen/hir_codegen.cpp
  codegen/todo_module.cpp
  lexer/lexer.cpp
  hir/hir_dump.cpp
  hir/lowering.cpp
  parser/ast_dump.cpp
  parser/parser.cpp
  sema/semantic_analyzer.cpp
  sema/symbol_table.cpp
  sema/type_system.cpp
)

target_include_directories(glsl2llvm_frontend
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(glsl2llvm_frontend
  PUBLIC
    glsl2llvm_llvm
)
