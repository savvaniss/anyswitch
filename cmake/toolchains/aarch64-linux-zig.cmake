# SPDX-License-Identifier: MIT
# Cross toolchain for the AArch64 Linux runtime, using Zig as clang + lld.
# The ZIG environment variable selects the zig binary (default: zig on PATH);
# tools/bootstrap/fetch_toolchain.sh writes an env.sh that sets it.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(_zig_dir "${CMAKE_CURRENT_LIST_DIR}/../zig")
set(CMAKE_C_COMPILER "${_zig_dir}/zig-cc")
set(CMAKE_CXX_COMPILER "${_zig_dir}/zig-c++")
set(CMAKE_AR "${_zig_dir}/zig-ar" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${_zig_dir}/zig-ranlib" CACHE FILEPATH "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
