# CMake toolchain for cross-compiling the Windows build of PolyWorks with
# mingw-w64 on a Unix host.
#
#   cmake -S . -B build-win \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
#         -DPW_WX_MSW_PREFIX=/path/to/extracted/wxMSW-package
#
# build_windows.sh drives this; see that script for the wxWidgets dependency.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# The win32 thread-model variant is deliberate.  The official wxWidgets MinGW
# binaries import libgcc_s_seh-1.dll and libstdc++-6.dll but not
# libwinpthread-1.dll, i.e. they are win32-threads builds; building this
# application with the posix-threads variant would put two different C++
# runtime configurations on either side of the wx ABI boundary.
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# Never search the host for anything: the Windows target must not pick up the
# host's headers or libraries.  wxWidgets is resolved explicitly from
# PW_WX_MSW_PREFIX rather than by find_package, precisely so that the host's
# wxGTK installation can never satisfy it (see cmake/wxMSWPrebuilt.cmake).
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
