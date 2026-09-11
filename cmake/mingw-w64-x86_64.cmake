# CMake toolchain for cross-compiling the Windows build of PolyWorks with
# mingw-w64 on a Unix host.
#
#   cmake -S . -B build-win \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
#
# build_windows.sh drives this.  The GUI dependencies (Dear ImGui, GLFW) are
# source-only and are fetched and compiled by cmake/DearImGui.cmake with this
# same compiler, so there is no prebuilt Windows package to point at.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# Never search the host for anything: the Windows target must not pick up the
# host's headers or libraries.  OpenGL comes from the mingw-w64 sysroot's
# opengl32, never from the host's libGL.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
