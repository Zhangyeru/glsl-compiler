add_executable(sema_test
  sema_test.cpp
)

target_link_libraries(sema_test
  PRIVATE
    glsl2llvm_frontend
)

set(SEMA_CASES
  sema_ok_basic
  sema_undefined_variable
  sema_duplicate_definition
  sema_type_mismatch_initializer
  sema_function_arg_count
  sema_function_arg_type
  sema_builtin_member_ok
  sema_builtin_member_invalid
  sema_return_type_mismatch
  sema_resolved_type_attached
)

foreach(SEMA_CASE IN LISTS SEMA_CASES)
  add_test(
    NAME sema.${SEMA_CASE}
    COMMAND sema_test --case ${SEMA_CASE}
  )
endforeach()
