cmake_minimum_required(VERSION 3.18)
project(stevelock C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(stevelock MODULE
  src/native/napi.c
  src/native/platform/darwin.c
  src/native/platform/linux.c
)

target_include_directories(stevelock PRIVATE
  src/native
  ${CMAKE_JS_INC}
)

target_link_libraries(stevelock PRIVATE ${CMAKE_JS_LIB})

set_target_properties(stevelock PROPERTIES
  PREFIX ""
  SUFFIX ".node"
)

if(APPLE)
  target_compile_options(stevelock PRIVATE -std=c11)
elseif(UNIX)
  target_compile_definitions(stevelock PRIVATE _GNU_SOURCE)
endif()
