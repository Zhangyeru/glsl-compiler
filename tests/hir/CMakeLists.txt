add_executable(hir_test
  hir_test.cpp
)

target_link_libraries(hir_test
  PRIVATE
    glsl2llvm_frontend
)

set(HIR_CASES
  hir_basic_binary
  hir_builtin_load
  hir_implicit_cast
  hir_vector_constructor
  hir_dump_control_flow
)

foreach(HIR_CASE IN LISTS HIR_CASES)
  add_test(
    NAME hir.${HIR_CASE}
    COMMAND hir_test --case ${HIR_CASE}
  )
endforeach()
