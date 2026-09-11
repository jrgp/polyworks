# Dear ImGui and GLFW, fetched at configure time.
#
# Both are pinned by version *and* SHA-256.  A pin without a checksum only says
# which URL was fetched, not what came back; upstream tags can be moved and
# GitHub's generated tarballs have changed bytes before.  The archives are
# cached under .deps/cache/ so a rebuild, and every one of the platform build
# scripts, reuses a single download.
#
# Neither project ships a usable CMake package for our purposes: Dear ImGui has
# no build system at all (it is meant to be compiled into the application), and
# GLFW's is used directly below.  Both are therefore built as ordinary static
# libraries of this project, which is also what keeps the shipped binaries free
# of extra runtime dependencies.

set(PW_IMGUI_VERSION "1.91.5")
set(PW_IMGUI_SHA256  "2aa2d169c569368439e5d5667e0796d09ca5cc6432965ce082e516937d7db254")
set(PW_GLFW_VERSION  "3.4")
set(PW_GLFW_SHA256   "c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01")

set(PW_DEPS_CACHE "${CMAKE_SOURCE_DIR}/.deps/cache" CACHE PATH
    "Directory holding downloaded third-party archives")

# Fetch (or reuse) one archive and extract it, verifying the checksum both on
# download and on reuse -- a cache entry truncated by an interrupted build would
# otherwise be trusted forever.
function(pw_fetch_archive name url sha256 out_dir_var)
  get_filename_component(_file "${url}" NAME)
  set(_archive "${PW_DEPS_CACHE}/${_file}")
  set(_dest "${CMAKE_BINARY_DIR}/_deps/${name}")

  if(EXISTS "${_archive}")
    file(SHA256 "${_archive}" _have)
    if(NOT _have STREQUAL "${sha256}")
      message(STATUS "${name}: cached archive has the wrong checksum, refetching")
      file(REMOVE "${_archive}")
    endif()
  endif()

  if(NOT EXISTS "${_archive}")
    message(STATUS "${name}: downloading ${url}")
    file(MAKE_DIRECTORY "${PW_DEPS_CACHE}")
    file(DOWNLOAD "${url}" "${_archive}"
         EXPECTED_HASH SHA256=${sha256}
         TLS_VERIFY ON
         STATUS _status
         SHOW_PROGRESS)
    list(GET _status 0 _code)
    if(NOT _code EQUAL 0)
      list(GET _status 1 _msg)
      file(REMOVE "${_archive}")
      message(FATAL_ERROR "${name}: download failed: ${_msg}")
    endif()
  endif()

  # The stamp records the checksum actually extracted, so bumping a version
  # re-extracts rather than silently building the previous one.
  set(_stamp "${_dest}/.pw-stamp")
  set(_want "")
  if(EXISTS "${_stamp}")
    file(READ "${_stamp}" _want)
  endif()
  if(NOT _want STREQUAL "${sha256}")
    message(STATUS "${name}: extracting")
    file(REMOVE_RECURSE "${_dest}")
    file(MAKE_DIRECTORY "${_dest}")
    file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_dest}")
    file(WRITE "${_stamp}" "${sha256}")
  endif()

  # Both archives unpack to a single <project>-<version> directory.
  file(GLOB _roots LIST_DIRECTORIES true "${_dest}/*")
  set(_root "")
  foreach(_candidate ${_roots})
    if(IS_DIRECTORY "${_candidate}")
      set(_root "${_candidate}")
      break()
    endif()
  endforeach()
  if(_root STREQUAL "")
    message(FATAL_ERROR "${name}: the archive contained no directory")
  endif()
  set(${out_dir_var} "${_root}" PARENT_SCOPE)
endfunction()

pw_fetch_archive(imgui
  "https://github.com/ocornut/imgui/archive/refs/tags/v${PW_IMGUI_VERSION}.tar.gz"
  "${PW_IMGUI_SHA256}" PW_IMGUI_DIR)
pw_fetch_archive(glfw
  "https://github.com/glfw/glfw/archive/refs/tags/${PW_GLFW_VERSION}.tar.gz"
  "${PW_GLFW_SHA256}" PW_GLFW_DIR)

message(STATUS "Dear ImGui ${PW_IMGUI_VERSION}: ${PW_IMGUI_DIR}")
message(STATUS "GLFW ${PW_GLFW_VERSION}: ${PW_GLFW_DIR}")

# ── GLFW ────────────────────────────────────────────────────────────────────
# Static, and with everything we do not use switched off.  The examples and
# tests drag in extra system libraries, and the documentation target needs
# doxygen; none of that should be able to break a PolyWorks build.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)
add_subdirectory("${PW_GLFW_DIR}" "${CMAKE_BINARY_DIR}/_build/glfw" EXCLUDE_FROM_ALL)

# GLFW is third-party code; its warnings are not ours to fix and would drown
# out our own under -Wall -Wextra -Wpedantic.
if(TARGET glfw AND NOT MSVC)
  target_compile_options(glfw PRIVATE -w)
endif()

# ── Dear ImGui ──────────────────────────────────────────────────────────────
# The core, plus the two backends we use.  imgui_demo.cpp is deliberately not
# compiled: it is 8k lines of sample code that would ship inside PolyWorks.exe.
add_library(imgui STATIC
  "${PW_IMGUI_DIR}/imgui.cpp"
  "${PW_IMGUI_DIR}/imgui_draw.cpp"
  "${PW_IMGUI_DIR}/imgui_tables.cpp"
  "${PW_IMGUI_DIR}/imgui_widgets.cpp"
  "${PW_IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
  "${PW_IMGUI_DIR}/backends/imgui_impl_opengl2.cpp"
)
target_include_directories(imgui PUBLIC
  "${PW_IMGUI_DIR}" "${PW_IMGUI_DIR}/backends")
target_link_libraries(imgui PUBLIC glfw)
if(NOT MSVC)
  target_compile_options(imgui PRIVATE -w)
endif()

# The OpenGL 2 backend, not the OpenGL 3 one.  PolyWorks' own renderer draws
# with fixed-function GL -- the most direct expression of what the original
# Direct3D 7 code does -- so the application already needs a compatibility
# context.  The GL2 backend draws the same way, which means no shader pipeline,
# no extension loader to link or ship, and one set of GL state for both halves
# of the frame.  It is also the backend most likely to work on the software
# rasterisers the Windows build gets tested on.
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC OpenGL::GL)
