cmake_minimum_required(VERSION 3.16)
project(PolyWorks VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── build type default ──────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# ── output directories ──────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── compiler warnings ───────────────────────────────────────────────────────
if(MSVC)
  add_compile_options(/W4 /wd4100 /wd4127)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# ── third-party: stb_image (header-only, vendored) ──────────────────────────
add_library(stb_image STATIC vendor/stb_image.c)
target_include_directories(stb_image PUBLIC vendor)
set_target_properties(stb_image PROPERTIES C_STANDARD 99)

# ── core library (no wxWidgets, no OpenGL) ──────────────────────────────────
file(GLOB_RECURSE CORE_SOURCES src-cpp/core/*.cpp)
add_library(pw_core STATIC ${CORE_SOURCES})
target_include_directories(pw_core PUBLIC src-cpp/core src-cpp)
target_link_libraries(pw_core PRIVATE stb_image)

# ── headless test executable ─────────────────────────────────────────────────
file(GLOB TEST_SOURCES src-cpp/tests/*.cpp)
if(TEST_SOURCES)
  # The asset resolver lives in texture_manager.cpp and is pure
  # std::filesystem; PW_RENDERER_NO_OPENGL compiles out the texture-upload half
  # so it can be tested without a GL library or a display.
  add_executable(pw_tests ${TEST_SOURCES} src-cpp/renderer/texture_manager.cpp)
  target_link_libraries(pw_tests PRIVATE pw_core stb_image)
  target_include_directories(pw_tests PRIVATE src-cpp/tests src-cpp/renderer src-cpp/ui)
  target_compile_definitions(pw_tests PRIVATE PW_RENDERER_NO_OPENGL=1)
  enable_testing()
  add_test(NAME headless COMMAND pw_tests)
endif()

# ── GUI application: Dear ImGui + GLFW + OpenGL ─────────────────────────────
include(${CMAKE_CURRENT_LIST_DIR}/cmake/DearImGui.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/NativeFileDialog.cmake)

file(GLOB UI_SOURCES src-cpp/ui/*.cpp)
file(GLOB RENDERER_SOURCES src-cpp/renderer/*.cpp)

add_executable(polyworks WIN32 MACOSX_BUNDLE ${UI_SOURCES} ${RENDERER_SOURCES})
target_include_directories(polyworks PRIVATE
  src-cpp src-cpp/core src-cpp/ui src-cpp/renderer)
target_link_libraries(polyworks PRIVATE pw_core stb_image imgui glfw OpenGL::GL)

# The native file chooser, where the platform has one worth using.
if(PW_HAVE_NFD)
  target_link_libraries(polyworks PRIVATE nfd)
  target_compile_definitions(polyworks PRIVATE PW_HAVE_NFD=1)
endif()

# macOS bundle setup
if(APPLE)
  set_target_properties(polyworks PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.opensoldat.polyworks"
    MACOSX_BUNDLE_BUNDLE_NAME "PolyWorks"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "PW.icns"
  )

  # The bundle icon is the application's own icon, converted from the Windows
  # resource the original installer has always shipped.  See
  # packaging/make-icns.py for why the conversion is done in Python rather than
  # with sips/iconutil.
  find_program(PW_PYTHON3 NAMES python3)
  if(PW_PYTHON3)
    set(PW_ICNS "${CMAKE_CURRENT_BINARY_DIR}/PW.icns")
    add_custom_command(OUTPUT "${PW_ICNS}"
      COMMAND "${PW_PYTHON3}" "${CMAKE_SOURCE_DIR}/packaging/make-icns.py"
              "${CMAKE_SOURCE_DIR}/installer/PW.ico" "${PW_ICNS}"
      DEPENDS "${CMAKE_SOURCE_DIR}/installer/PW.ico"
              "${CMAKE_SOURCE_DIR}/packaging/make-icns.py"
      COMMENT "Generating PW.icns from installer/PW.ico"
      VERBATIM)
    target_sources(polyworks PRIVATE "${PW_ICNS}")
    set_source_files_properties("${PW_ICNS}" PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources)
  else()
    message(WARNING "python3 not found — the .app will have no icon")
  endif()

  # Skins, cursors and the notfound placeholder go where a Mac application
  # keeps its data, so that the bundle can be dragged anywhere and still find
  # them (see skinsPath() in src-cpp/ui/platform.cpp).
  add_custom_command(TARGET polyworks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/installer/skins"
            "$<TARGET_BUNDLE_CONTENT_DIR:polyworks>/Resources/skins"
    COMMENT "Copying skins into the application bundle"
    VERBATIM)
endif()

# Windows resources (icon, version info, DPI-aware manifest)
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/src-cpp/ui/polyworks.rc")
  target_sources(polyworks PRIVATE src-cpp/ui/polyworks.rc)
  set_source_files_properties(src-cpp/ui/polyworks.rc PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/src-cpp/ui")
endif()

# The shipped executable is PolyWorks.exe, as in the original installer.
# -mwindows suppresses the console window MinGW would otherwise attach.  With
# wxWidgets gone there is no third-party DLL importing libstdc++, so the GCC
# runtime is linked statically and the distribution needs no compiler DLLs.
if(WIN32)
  set_target_properties(polyworks PROPERTIES OUTPUT_NAME "PolyWorks")
  if(MINGW)
    target_link_options(polyworks PRIVATE
      -mwindows -static-libgcc -static-libstdc++ -static)
  endif()
endif()

# ── install skins/assets alongside binary ───────────────────────────────────
install(TARGETS polyworks DESTINATION .)
install(DIRECTORY installer/skins DESTINATION .)
