add_executable(todo_module_test
  todo_module_test.cpp
)

target_link_libraries(todo_module_test
  PRIVATE
    glsl2llvm_frontend
)

add_test(NAME unit.todo_module COMMAND todo_module_test)

add_test(NAME cli.help COMMAND $<TARGET_FILE:glsl2llvm> --help)
set_tests_properties(cli.help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: .+ input\\.comp -S -o out\\.ll")

if(GLSL2LLVM_HAS_LLVM_CPP_API)
  set(CLI_SMOKE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cli-smoke.ll")
  add_test(NAME cli.smoke
    COMMAND $<TARGET_FILE:glsl2llvm> ${PROJECT_SOURCE_DIR}/examples/minimal.comp -S -o ${CLI_SMOKE_OUTPUT}
  )

  add_test(NAME cli.output_file
    COMMAND ${CMAKE_COMMAND} -DFILE_PATH=${CLI_SMOKE_OUTPUT} -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ir_file.cmake
  )
  set_tests_properties(cli.output_file PROPERTIES DEPENDS cli.smoke)
else()
  message(STATUS "Skipping CLI IR smoke tests: LLVM C++ API not available")
endif()

add_subdirectory(parser)
add_subdirectory(sema)
add_subdirectory(hir)
if(GLSL2LLVM_HAS_LLVM_CPP_API)
  add_subdirectory(codegen)
  add_subdirectory(e2e)
  add_subdirectory(diff)
else()
  message(STATUS "Skipping codegen/e2e/diff tests: LLVM C++ API not available")
endif()
