set(E2E_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/example.ll")

add_test(NAME e2e.compute_shader
  COMMAND $<TARGET_FILE:glsl2llvm> ${PROJECT_SOURCE_DIR}/examples/example.comp -S -o ${E2E_OUTPUT}
)

add_test(NAME e2e.compute_ir_check
  COMMAND ${CMAKE_COMMAND}
    -DFILE_PATH=${E2E_OUTPUT}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_compute_ir.cmake
)
set_tests_properties(e2e.compute_ir_check PROPERTIES DEPENDS e2e.compute_shader)
