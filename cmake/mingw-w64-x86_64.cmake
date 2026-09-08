# CMake toolchain for cross-compiling the portable Windows build of PolyWorks
# with mingw-w64 on a Unix host.
#
#   cmake -S . -B build-win \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
#         -DwxWidgets_CONFIG_EXECUTABLE=/path/to/mingw/wx-config
#
# The resulting PolyWorks.exe is statically linked against wxWidgets and the
# GCC runtime so that the distribution needs no non-system DLLs at all.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# Additional sysroots holding cross-built dependencies (e.g. the mingw
# wxWidgets prefix).  FindwxWidgets re-checks every -l library reported by
# wx-config with find_library(); with FIND_ROOT_PATH_MODE_LIBRARY set to ONLY
# those lookups are confined to CMAKE_FIND_ROOT_PATH, so a prefix outside it
# makes wxWidgets appear "not found" even though wx-config works perfectly.
if(PW_MINGW_PREFIX)
  list(APPEND CMAKE_FIND_ROOT_PATH ${PW_MINGW_PREFIX})
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# BOTH rather than ONLY: cross-built dependencies such as the mingw wxWidgets
# prefix live outside the sysroot, and ONLY re-roots every candidate path onto
# CMAKE_FIND_ROOT_PATH instead of searching it as given.  The library and
# header names are architecture-qualified, so there is no risk of picking up
# host artefacts.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
