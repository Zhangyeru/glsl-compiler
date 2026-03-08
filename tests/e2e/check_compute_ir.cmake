if(NOT DEFINED FILE_PATH)
  message(FATAL_ERROR "FILE_PATH is required")
endif()

if(NOT EXISTS "${FILE_PATH}")
  message(FATAL_ERROR "Expected IR output file does not exist: ${FILE_PATH}")
endif()

file(READ "${FILE_PATH}" IR_CONTENT)

macro(require_pattern PATTERN LABEL)
  string(FIND "${IR_CONTENT}" "${PATTERN}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "Missing IR pattern (${LABEL}): ${PATTERN}")
  endif()
endmacro()

require_pattern("define void @main(i32 %global_id_x, ptr %data)" "entry signature")
require_pattern("getelementptr" "pointer arithmetic")
require_pattern("load float" "buffer load")
require_pattern("store float" "buffer store")
require_pattern("fmul float" "binary multiply")
