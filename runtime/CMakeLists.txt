# SPDX-License-Identifier: MIT
# libnxrt.so: the native replacement for a title's `sdk` module (and for the
# kernel services its code reaches). Cross-compiled for AArch64 Linux:
#
#   cmake -S runtime -B build-runtime --toolchain cmake/toolchains/aarch64-linux-zig.cmake
#
cmake_minimum_required(VERSION 3.24)
project(anyswitch_runtime LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
  message(FATAL_ERROR "The runtime targets AArch64. Configure with "
                      "--toolchain cmake/toolchains/aarch64-linux-zig.cmake (see docs/dev/BUILD.md).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(NXRT_WERROR "Treat compiler warnings as errors" OFF)

# One directory per guest API module; each has docs/dev/debt/<module>.md.
set(NXRT_MODULES rt nn_os libc cxxabi)
set(NXRT_SOURCES)
foreach(module IN LISTS NXRT_MODULES)
  file(GLOB module_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/${module}/*.cpp")
  list(APPEND NXRT_SOURCES ${module_sources})
endforeach()

add_library(nxrt SHARED ${NXRT_SOURCES})
target_include_directories(nxrt PRIVATE include ../core/abi/include)
target_compile_options(nxrt PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden -Wall -Wextra
                                    -Wno-missing-field-initializers -Wno-return-type-c-linkage)
if(NXRT_WERROR)
  target_compile_options(nxrt PRIVATE -Werror)
endif()
target_link_options(nxrt PRIVATE
  "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/nxrt.map"
  "-Wl,-z,now"
  "-Wl,-z,defs")
target_link_libraries(nxrt PRIVATE pthread dl)
set_target_properties(nxrt PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/nxrt.map")

install(TARGETS nxrt LIBRARY DESTINATION lib)
