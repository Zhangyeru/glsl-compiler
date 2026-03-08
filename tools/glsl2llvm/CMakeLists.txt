add_executable(glsl2llvm
  main.cpp
)

target_link_libraries(glsl2llvm
  PRIVATE
    glsl2llvm_frontend
)

target_include_directories(glsl2llvm
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
