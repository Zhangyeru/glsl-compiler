find_package(Python3 COMPONENTS Interpreter REQUIRED)

find_program(GLSLANG_VALIDATOR_EXECUTABLE
  NAMES glslangValidator glslang
  HINTS /tmp/glslang-local/root/usr/bin
)

if(GLSLANG_VALIDATOR_EXECUTABLE)
  set(GLSLANG_VALIDATOR_FOR_TEST "${GLSLANG_VALIDATOR_EXECUTABLE}")
else()
  set(GLSLANG_VALIDATOR_FOR_TEST "glslangValidator")
endif()

set(DIFF_CASES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cases.json")
set(DIFF_REPORT_FILE "${CMAKE_CURRENT_BINARY_DIR}/compatibility-report.md")

add_test(NAME diff.glslang_differential
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_diff_tests.py
    --glsl2llvm $<TARGET_FILE:glsl2llvm>
    --glslang ${GLSLANG_VALIDATOR_FOR_TEST}
    --cases ${DIFF_CASES_FILE}
    --work-dir ${CMAKE_CURRENT_BINARY_DIR}
    --report ${DIFF_REPORT_FILE}
)

set_tests_properties(diff.glslang_differential PROPERTIES
  SKIP_RETURN_CODE 125
)
