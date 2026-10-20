# SPDX-License-Identifier: MIT
# Host tools: the relinker and its unit tests. The AArch64 runtime is a
# separate project in runtime/ (it is cross-compiled; see docs/dev/BUILD.md).
cmake_minimum_required(VERSION 3.24)
project(anyswitch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ANYSWITCH_WERROR "Treat compiler warnings as errors" OFF)
option(ANYSWITCH_BUILD_TESTS "Build unit tests" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
  if(ANYSWITCH_WERROR)
    add_compile_options(/WX)
  endif()
else()
  add_compile_options(-Wall -Wextra -Wshadow -Wno-missing-field-initializers)
  if(ANYSWITCH_WERROR)
    add_compile_options(-Werror)
  endif()
endif()

add_library(nx_abi INTERFACE)
target_include_directories(nx_abi INTERFACE core/abi/include)

add_library(nx_common STATIC
  core/common/src/file.cpp
  core/common/src/sha256.cpp)
target_include_directories(nx_common PUBLIC core/common/include)

add_library(nx_format STATIC
  core/format/src/detect.cpp
  core/format/src/elf.cpp
  core/format/src/elf_reader.cpp
  core/format/src/lz4.cpp
  core/format/src/module.cpp
  core/format/src/npdm.cpp)
target_include_directories(nx_format PUBLIC core/format/include)
target_link_libraries(nx_format PUBLIC nx_common)

add_library(nx_relinker STATIC
  core/relinker/src/census.cpp
  core/relinker/src/patcher.cpp
  core/relinker/src/registry.cpp
  core/relinker/src/writer.cpp)
target_include_directories(nx_relinker PUBLIC core/relinker/include)
target_link_libraries(nx_relinker PUBLIC nx_format nx_abi)

add_executable(anyswitch-relink core/relinker/main.cpp)
target_link_libraries(anyswitch-relink PRIVATE nx_relinker)
if(MINGW)
  target_link_options(anyswitch-relink PRIVATE -static)
endif()

if(ANYSWITCH_BUILD_TESTS)
  enable_testing()
  add_executable(nx_unit_tests
    tests/unit/test_main.cpp
    tests/unit/test_a64.cpp
    tests/unit/test_codecs.cpp
    tests/unit/test_relink.cpp)
  target_link_libraries(nx_unit_tests PRIVATE nx_relinker)
  if(MINGW)
    target_link_options(nx_unit_tests PRIVATE -static)
  endif()
  add_test(NAME unit COMMAND nx_unit_tests)
endif()

install(TARGETS anyswitch-relink RUNTIME DESTINATION bin)
