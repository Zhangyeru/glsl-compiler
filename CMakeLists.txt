cmake_minimum_required(VERSION 3.20)
project(glsl2llvm LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GLSL2LLVM_REQUIRE_LLVM_CONFIG "Require LLVM CMake package (CONFIG mode)." OFF)

find_package(LLVM CONFIG QUIET)

add_library(glsl2llvm_llvm INTERFACE)
set(GLSL2LLVM_HAS_LLVM_CPP_API OFF)

if(LLVM_FOUND)
  message(STATUS "Using LLVM from CMake package: ${LLVM_PACKAGE_VERSION}")
  target_compile_definitions(glsl2llvm_llvm INTERFACE GLSL2LLVM_HAVE_LLVM_CONFIG=1)
  set(GLSL2LLVM_HAS_LLVM_CPP_API ON)

  if(LLVM_INCLUDE_DIRS)
    target_include_directories(glsl2llvm_llvm INTERFACE ${LLVM_INCLUDE_DIRS})
  endif()

  set(GLSL2LLVM_LLVM_LIBS "")
  if(COMMAND llvm_map_components_to_libnames)
    llvm_map_components_to_libnames(GLSL2LLVM_LLVM_LIBS Core Support)
  endif()

  if(GLSL2LLVM_LLVM_LIBS)
    target_link_libraries(glsl2llvm_llvm INTERFACE ${GLSL2LLVM_LLVM_LIBS})
  elseif(TARGET LLVM)
    target_link_libraries(glsl2llvm_llvm INTERFACE LLVM)
  else()
    message(FATAL_ERROR "LLVM package found but no link targets are available.")
  endif()
else()
  if(GLSL2LLVM_REQUIRE_LLVM_CONFIG)
    message(FATAL_ERROR "LLVM CMake package not found. Set LLVM_DIR to your LLVM cmake folder.")
  endif()

  find_library(GLSL2LLVM_LLVM_SHARED_LIB
    NAMES LLVM LLVM-18 LLVM-17 LLVM-16
    PATHS /usr/lib/llvm-18/lib /usr/lib/llvm-17/lib /usr/lib/llvm-16/lib
  )

  if(NOT GLSL2LLVM_LLVM_SHARED_LIB)
    message(FATAL_ERROR "Unable to find LLVM shared library. Install LLVM dev/runtime package or set LLVM_DIR.")
  endif()

  message(WARNING "LLVM CMake package not found. Falling back to LLVM C API dynamic linking: ${GLSL2LLVM_LLVM_SHARED_LIB}")
  target_compile_definitions(glsl2llvm_llvm INTERFACE GLSL2LLVM_USE_LLVM_C_API_FALLBACK=1)
  target_link_libraries(glsl2llvm_llvm INTERFACE ${GLSL2LLVM_LLVM_SHARED_LIB})
endif()

add_subdirectory(lib)
add_subdirectory(tools)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
