add_executable(codegen_test
  codegen_test.cpp
)

target_link_libraries(codegen_test
  PRIVATE
    glsl2llvm_frontend
)

set(CODEGEN_CASES
  ir_binary_ops
  ir_builtin_global_id_x
  ir_if_for_control_flow
  ir_buffer_load_store
  ir_vec_type_mapping
)

foreach(CODEGEN_CASE IN LISTS CODEGEN_CASES)
  add_test(
    NAME codegen.${CODEGEN_CASE}
    COMMAND codegen_test --case ${CODEGEN_CASE}
  )
endforeach()
