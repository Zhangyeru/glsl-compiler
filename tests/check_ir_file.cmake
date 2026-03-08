if(NOT DEFINED FILE_PATH)
  message(FATAL_ERROR "FILE_PATH is required")
endif()

if(NOT EXISTS "${FILE_PATH}")
  message(FATAL_ERROR "Expected IR output file does not exist: ${FILE_PATH}")
endif()

file(READ "${FILE_PATH}" IR_CONTENT)
string(FIND "${IR_CONTENT}" "source_filename" SOURCE_FILENAME_POS)
if(SOURCE_FILENAME_POS EQUAL -1)
  message(FATAL_ERROR "IR output does not contain source_filename")
endif()
