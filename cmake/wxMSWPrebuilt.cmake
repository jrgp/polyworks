# wxMSWPrebuilt.cmake — use an official wxWidgets Windows/MinGW-w64 binary package.
#
# Set PW_WX_MSW_PREFIX to the root of an extracted official package, i.e. the
# directory produced by unpacking these three archives from
# https://github.com/wxWidgets/wxWidgets/releases into one place:
#
#   wxWidgets-<ver>-headers.7z          -> include/wx/...
#   wxMSW-<ver>_gcc<abi>_x64_Dev.7z     -> lib/gcc<abi>_x64_dll/*.a + mswu/wx/setup.h
#   wxMSW-<ver>_gcc<abi>_x64_ReleaseDLL -> lib/gcc<abi>_x64_dll/*.dll
#
# build_windows.sh does the downloading, checksum verification and extraction.
#
# Why not find_package(wxWidgets)?  CMake's FindwxWidgets chooses between a
# "win32" search style (headers + import libraries, what a Windows binary
# package provides) and a "unix" style (delegating to the wx-config script).
# The condition is
#
#   if(WIN32 AND NOT CYGWIN AND NOT MSYS AND NOT CMAKE_CROSSCOMPILING)
#
# so a cross build always takes the "unix" branch and runs whatever wx-config
# is on PATH.  On a Linux host that is the distribution's wxGTK, which would
# quietly link the Windows executable against GTK headers and produce a broken
# binary.  Resolving the prebuilt package here keeps the Windows dependency
# chain strictly wxMSW -> Win32.

set(_wx_prefix "${PW_WX_MSW_PREFIX}")

if(NOT IS_DIRECTORY "${_wx_prefix}")
  message(FATAL_ERROR "PW_WX_MSW_PREFIX is not a directory: ${_wx_prefix}")
endif()

if(NOT EXISTS "${_wx_prefix}/include/wx/wx.h")
  message(FATAL_ERROR
    "No wxWidgets headers under ${_wx_prefix}/include/wx. "
    "Extract wxWidgets-<version>-headers.7z into ${_wx_prefix}.")
endif()

# The library directory is named after the compiler ABI the package was built
# with (lib/gcc1220_x64_dll).  Glob rather than hard-code it so the pinned
# version in build_windows.sh can be bumped without touching this file.
file(GLOB _wx_lib_dirs "${_wx_prefix}/lib/gcc*_x64_dll")
list(LENGTH _wx_lib_dirs _wx_lib_dir_count)
if(_wx_lib_dir_count EQUAL 0)
  message(FATAL_ERROR
    "No lib/gcc*_x64_dll directory under ${_wx_prefix}. "
    "Extract the x64 Dev and ReleaseDLL archives into ${_wx_prefix}.")
elseif(_wx_lib_dir_count GREATER 1)
  message(FATAL_ERROR
    "Several wxWidgets ABI directories under ${_wx_prefix}/lib: ${_wx_lib_dirs}. "
    "Keep one package per prefix so the DLLs that get shipped are unambiguous.")
endif()
list(GET _wx_lib_dirs 0 wxWidgets_MSW_LIB_DIR)

# Each binary package carries its own wx/setup.h describing exactly how it was
# configured.  Compiling against a different one silently changes the ABI, so
# this must be the setup.h from the very same package as the import libraries.
if(NOT EXISTS "${wxWidgets_MSW_LIB_DIR}/mswu/wx/setup.h")
  message(FATAL_ERROR
    "No mswu/wx/setup.h in ${wxWidgets_MSW_LIB_DIR}; the Dev archive is missing "
    "or was extracted somewhere else.")
endif()

# Release (non-debug) Unicode DLL build: libwxbase32u.a, libwxmsw32u_core.a, ...
set(_wx_components gl core base)
set(wxWidgets_LIBRARIES "")
foreach(_comp IN LISTS _wx_components)
  if(_comp STREQUAL "base")
    set(_wx_libname "wxbase32u")
  else()
    set(_wx_libname "wxmsw32u_${_comp}")
  endif()
  set(_wx_libfile "${wxWidgets_MSW_LIB_DIR}/lib${_wx_libname}.a")
  if(NOT EXISTS "${_wx_libfile}")
    message(FATAL_ERROR "Missing wxWidgets import library: ${_wx_libfile}")
  endif()
  list(APPEND wxWidgets_LIBRARIES "${_wx_libfile}")
endforeach()

# Windows libraries wxMSW's headers and our own code call into directly.
list(APPEND wxWidgets_LIBRARIES
  opengl32 glu32 comctl32 rpcrt4 shlwapi version uxtheme oleacc
  ole32 oleaut32 uuid winspool winmm comdlg32 advapi32 shell32
  gdi32 user32 kernel32)

set(wxWidgets_INCLUDE_DIRS
  "${wxWidgets_MSW_LIB_DIR}/mswu"   # setup.h first: it must win over any other
  "${_wx_prefix}/include")

# WXUSINGDLL is what switches the headers to importing from the wx DLLs; it is
# required and cannot be inferred from the headers alone.
set(wxWidgets_DEFINITIONS __WXMSW__ WXUSINGDLL UNICODE _UNICODE)

set(wxWidgets_FOUND TRUE)

# Read the version out of the headers purely so the configure output is
# informative and a mismatched prefix is obvious.
file(READ "${_wx_prefix}/include/wx/version.h" _wx_version_h)
string(REGEX MATCH "#define +wxMAJOR_VERSION +([0-9]+)" _m "${_wx_version_h}")
set(_wx_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define +wxMINOR_VERSION +([0-9]+)" _m "${_wx_version_h}")
set(_wx_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define +wxRELEASE_NUMBER +([0-9]+)" _m "${_wx_version_h}")
set(_wx_release "${CMAKE_MATCH_1}")
set(wxWidgets_VERSION_STRING "${_wx_major}.${_wx_minor}.${_wx_release}")

get_filename_component(_wx_abi "${wxWidgets_MSW_LIB_DIR}" NAME)
message(STATUS
  "wxWidgets ${wxWidgets_VERSION_STRING} (prebuilt wxMSW, ${_wx_abi}) at ${_wx_prefix}")
