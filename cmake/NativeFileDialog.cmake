# Native File Dialog Extended — the platform's own file chooser.
#
# PolyWorks draws its entire interface itself, which is right for the editor
# but wrong for choosing a file: the ImGui browser cannot offer the sidebar,
# the recent places, the network volumes, the search field or the keyboard
# conventions a user already knows, and on macOS it cannot reach anything
# behind the sandbox's file-access prompts.  The original used the Windows
# common dialog (frmOpenSoldatMapEditor's CommonDialog control), so a native
# chooser is also the behaviour being preserved.
#
# The library is used on Windows and macOS only.  Its Linux backends need
# either GTK or a D-Bus portal, and pulling GTK into this project would undo
# the whole point of the ImGui migration, so Linux keeps the built-in browser.
# That is also why this is fetched separately from the ImGui dependencies
# rather than alongside them.
#
# Pinned by version and SHA-256, cached, and extracted by the same
# pw_fetch_archive() that DearImGui.cmake defines -- include that first.

set(PW_NFD_VERSION "1.2.1")
set(PW_NFD_SHA256  "443697a857c4efacbe08cdaf5182724fa9d9b9a79b8feff2a1601bde1df46b07")

if(WIN32 OR APPLE)
  pw_fetch_archive(nfd
    "https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v${PW_NFD_VERSION}.tar.gz"
    "${PW_NFD_SHA256}" PW_NFD_DIR)
  message(STATUS "Native File Dialog Extended ${PW_NFD_VERSION}: ${PW_NFD_DIR}")

  # nfd_cocoa.m is Objective-C; CMake will not compile a .m file unless the
  # language is enabled, and NFD's own CMakeLists does not enable it because
  # it expects to be the top-level project.
  if(APPLE)
    enable_language(OBJC)
  endif()

  set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(NFD_INSTALL     OFF CACHE BOOL "" FORCE)
  add_subdirectory("${PW_NFD_DIR}" "${CMAKE_BINARY_DIR}/_build/nfd" EXCLUDE_FROM_ALL)

  if(TARGET nfd AND NOT MSVC)
    target_compile_options(nfd PRIVATE -w)
  endif()

  set(PW_HAVE_NFD ON)
else()
  set(PW_HAVE_NFD OFF)
endif()
