cmake_minimum_required(VERSION 3.18)
project(stevelock C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(STEVELOCK_TEST_ASAN "Enable AddressSanitizer for test binaries" OFF)

enable_testing()

add_library(stevelock MODULE
  src/native/napi.c
)

add_library(stevelock_native STATIC
  src/native/stevelock.c
)

set_target_properties(stevelock_native PROPERTIES
  OUTPUT_NAME stevelock
)

target_include_directories(stevelock PRIVATE
  src/native
  ${CMAKE_JS_INC}
)

target_link_libraries(stevelock PRIVATE ${CMAKE_JS_LIB})

target_include_directories(stevelock_native PUBLIC
  src/native
)

set_target_properties(stevelock PROPERTIES
  PREFIX ""
  SUFFIX ".node"
)

if(APPLE)
  target_compile_options(stevelock PRIVATE -std=c11)
  target_link_options(stevelock PRIVATE -undefined dynamic_lookup)
elseif(UNIX)
  target_compile_definitions(stevelock PRIVATE _GNU_SOURCE)
  target_compile_definitions(stevelock_native PRIVATE _GNU_SOURCE)
endif()

add_executable(stevelock_process_test src/test/process.c)
add_executable(stevelock_lifecycle_test src/test/lifecycle.c)
add_executable(stevelock_sandbox_test src/test/sandbox.c)

add_executable(stevelock_net_probe
  src/test/bin/net.c
)

add_executable(stevelock_testbox
  src/test/bin/box.c
)

target_include_directories(stevelock_process_test PRIVATE src/test/include src/native)
target_include_directories(stevelock_lifecycle_test PRIVATE src/test/include src/native)
target_include_directories(stevelock_sandbox_test PRIVATE src/test/include src/native)

target_include_directories(stevelock_net_probe PRIVATE
  src/test/include
)

target_include_directories(stevelock_testbox PRIVATE
  src/test/include
)

if(UNIX AND NOT APPLE)
  target_compile_definitions(stevelock_process_test PRIVATE _GNU_SOURCE)
  target_compile_definitions(stevelock_lifecycle_test PRIVATE _GNU_SOURCE)
  target_compile_definitions(stevelock_sandbox_test PRIVATE _GNU_SOURCE)
endif()

if(STEVELOCK_TEST_ASAN)
  target_compile_options(stevelock_process_test PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_options(stevelock_process_test PRIVATE -fsanitize=address)
  target_compile_options(stevelock_lifecycle_test PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_options(stevelock_lifecycle_test PRIVATE -fsanitize=address)
  target_compile_options(stevelock_sandbox_test PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_options(stevelock_sandbox_test PRIVATE -fsanitize=address)
  target_compile_options(stevelock_net_probe PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_options(stevelock_net_probe PRIVATE -fsanitize=address)
  target_compile_options(stevelock_testbox PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_options(stevelock_testbox PRIVATE -fsanitize=address)
endif()

install(DIRECTORY DESTINATION bin)

install(TARGETS stevelock_native
  ARCHIVE DESTINATION lib
)

install(FILES src/native/stevelock.h
  DESTINATION include
)
