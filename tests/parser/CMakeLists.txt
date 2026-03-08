add_executable(lexer_test
  lexer_test.cpp
)

target_link_libraries(lexer_test
  PRIVATE
    glsl2llvm_frontend
)

set(LEXER_CASES
  keywords
  symbols
  identifiers
  numeric_literals
  mixed_snippet
  source_location
  peek
  error_token
  slash_minus
  dot_number_boundary
)

foreach(LEXER_CASE IN LISTS LEXER_CASES)
  add_test(
    NAME lexer.${LEXER_CASE}
    COMMAND lexer_test --case ${LEXER_CASE}
  )
endforeach()

add_executable(parser_test
  parser_test.cpp
)

target_link_libraries(parser_test
  PRIVATE
    glsl2llvm_frontend
)

set(PARSER_CASES
  function_basic
  function_with_return_binary
  two_functions
  local_var_init
  local_var_no_init
  nested_block
  if_stmt
  for_stmt_var_init
  for_stmt_expr_init
  call_expr
  member_expr
  precedence_mul_add
  precedence_paren
  ast_dump
  parse_error_missing_semicolon
)

foreach(PARSER_CASE IN LISTS PARSER_CASES)
  add_test(
    NAME parser.${PARSER_CASE}
    COMMAND parser_test --case ${PARSER_CASE}
  )
endforeach()
