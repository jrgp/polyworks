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
  add_executable(pw_tests ${TEST_SOURCES})
  target_link_libraries(pw_tests PRIVATE pw_core)
  target_include_directories(pw_tests PRIVATE src-cpp/tests)
  enable_testing()
  add_test(NAME headless COMMAND pw_tests)
endif()

# ── wxWidgets GUI application (optional; core + tests build without it) ─────
find_package(wxWidgets COMPONENTS core base gl)
include(${wxWidgets_USE_FILE})

# OpenGL
find_package(OpenGL REQUIRED)

file(GLOB_RECURSE GUI_SOURCES src-cpp/gui/*.cpp src-cpp/renderer/*.cpp)
file(GLOB_RECURSE APP_SOURCES src-cpp/app/*.cpp)

if(wxWidgets_FOUND AND (GUI_SOURCES OR APP_SOURCES))
  include(${wxWidgets_USE_FILE})
  find_package(OpenGL REQUIRED)

  add_executable(polyworks WIN32 MACOSX_BUNDLE
    ${GUI_SOURCES}
    ${APP_SOURCES}
  )
  target_include_directories(polyworks PRIVATE src-cpp src-cpp/core src-cpp/gui src-cpp/renderer)
  target_link_libraries(polyworks PRIVATE pw_core stb_image ${wxWidgets_LIBRARIES} OpenGL::GL)

  # macOS bundle setup
  if(APPLE)
    set_target_properties(polyworks PROPERTIES
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.opensoldat.polyworks"
      MACOSX_BUNDLE_BUNDLE_NAME "PolyWorks"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
      MACOSX_BUNDLE_ICON_FILE "PW.icns"
    )
  endif()

  # Windows resources
  if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/src-cpp/app/polyworks.rc")
    target_sources(polyworks PRIVATE src-cpp/app/polyworks.rc)
  endif()

  # ── install skins/assets alongside binary ─────────────────────────────────
  install(TARGETS polyworks DESTINATION .)
  install(DIRECTORY installer/skins DESTINATION .)
else()
  message(STATUS "wxWidgets not found (or no GUI sources yet) — skipping GUI target")
endif()
