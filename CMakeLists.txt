cmake_minimum_required(VERSION 3.25)

project(rl_harfbuzz_textlib VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CMakePackageConfigHelpers)
include(FetchContent)
include(GNUInstallDirs)

option(RLHB_BUILD_EXAMPLES "Build rl_harfbuzz_textlib examples" ON)
option(RLHB_BUILD_TESTS "Build rl_harfbuzz_textlib tests" OFF)
option(RLHB_FETCH_RAYLIB "Fetch raylib if no compatible target is already available" ON)
option(RLHB_FETCH_HARFBUZZ "Fetch HarfBuzz if no compatible target is already available" ON)
option(RLHB_BUNDLE_DEFAULT_FONT "Embed a bundled default font into the library" ON)

set(RLHB_DEFAULT_FONT_LICENSE_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/LICENSE.Amiri.txt"
  CACHE FILEPATH
  "License file for the bundled default font"
)
set(RLHB_DEFAULT_FONT_HEADER
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rlhb_default_font_data.h"
  CACHE FILEPATH
  "Pre-generated header that embeds the bundled default font bytes"
)

function(rlhb_ensure_dependency_bridge bridge_name target_name)
  if(TARGET ${bridge_name} OR NOT TARGET ${target_name})
    return()
  endif()

  add_library(${bridge_name} INTERFACE IMPORTED GLOBAL)
  set_target_properties(${bridge_name} PROPERTIES
    INTERFACE_LINK_LIBRARIES ${target_name}
  )
endfunction()

function(rlhb_find_or_fetch_raylib)
  if(TARGET raylib OR TARGET raylib::raylib)
    rlhb_ensure_dependency_bridge(raylib::raylib raylib)
    return()
  endif()

  find_package(raylib QUIET CONFIG)
  if(TARGET raylib OR TARGET raylib::raylib)
    rlhb_ensure_dependency_bridge(raylib::raylib raylib)
    return()
  endif()

  if(NOT RLHB_FETCH_RAYLIB)
    message(FATAL_ERROR "raylib was not found. Provide raylib::raylib or enable RLHB_FETCH_RAYLIB.")
  endif()

  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(raylib)
  rlhb_ensure_dependency_bridge(raylib::raylib raylib)
endfunction()

function(rlhb_find_or_fetch_harfbuzz)
  if((TARGET harfbuzz OR TARGET harfbuzz::harfbuzz) AND (TARGET harfbuzz-gpu OR TARGET harfbuzz::harfbuzz-gpu))
    rlhb_ensure_dependency_bridge(harfbuzz::harfbuzz harfbuzz)
    rlhb_ensure_dependency_bridge(harfbuzz::harfbuzz-gpu harfbuzz-gpu)
    return()
  endif()

  find_package(harfbuzz QUIET CONFIG)
  if((TARGET harfbuzz OR TARGET harfbuzz::harfbuzz) AND (TARGET harfbuzz-gpu OR TARGET harfbuzz::harfbuzz-gpu))
    rlhb_ensure_dependency_bridge(harfbuzz::harfbuzz harfbuzz)
    rlhb_ensure_dependency_bridge(harfbuzz::harfbuzz-gpu harfbuzz-gpu)
    return()
  endif()

  if(NOT RLHB_FETCH_HARFBUZZ)
    message(FATAL_ERROR "A HarfBuzz package with GPU support was not found. Provide harfbuzz::harfbuzz and harfbuzz::harfbuzz-gpu or enable RLHB_FETCH_HARFBUZZ.")
  endif()

  set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
  set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
  set(HB_BUILD_RASTER OFF CACHE BOOL "" FORCE)
  set(HB_BUILD_VECTOR OFF CACHE BOOL "" FORCE)
  set(HB_BUILD_GPU ON CACHE BOOL "" FORCE)
  set(HB_HAVE_GOBJECT OFF CACHE BOOL "" FORCE)
  set(HB_HAVE_INTROSPECTION OFF CACHE BOOL "" FORCE)
  set(HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE)
  set(HB_HAVE_GLIB OFF CACHE BOOL "" FORCE)
  set(HB_HAVE_ICU OFF CACHE BOOL "" FORCE)
  set(HB_HAVE_GRAPHITE2 OFF CACHE BOOL "" FORCE)
  if(WIN32)
    set(HB_HAVE_UNISCRIBE OFF CACHE BOOL "" FORCE)
    set(HB_HAVE_GDI OFF CACHE BOOL "" FORCE)
    set(HB_HAVE_DIRECTWRITE OFF CACHE BOOL "" FORCE)
  endif()

  FetchContent_Declare(
    harfbuzz
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/harfbuzz/harfbuzz/archive/refs/tags/14.0.0.tar.gz
  )
  FetchContent_MakeAvailable(harfbuzz)

  rlhb_ensure_dependency_bridge(harfbuzz::harfbuzz harfbuzz)
  rlhb_ensure_dependency_bridge(harfbuzz::harfbuzz-gpu harfbuzz-gpu)
endfunction()

rlhb_find_or_fetch_raylib()
rlhb_find_or_fetch_harfbuzz()

if(NOT TARGET raylib::raylib)
  message(FATAL_ERROR "raylib target resolution failed.")
endif()

if(NOT TARGET harfbuzz::harfbuzz OR NOT TARGET harfbuzz::harfbuzz-gpu)
  message(FATAL_ERROR "HarfBuzz GPU target resolution failed. A package without harfbuzz-gpu is not sufficient.")
endif()

if(RLHB_BUNDLE_DEFAULT_FONT)
  if(NOT EXISTS "${RLHB_DEFAULT_FONT_HEADER}")
    message(FATAL_ERROR "RLHB_BUNDLE_DEFAULT_FONT is ON but RLHB_DEFAULT_FONT_HEADER does not exist: ${RLHB_DEFAULT_FONT_HEADER}")
  endif()
endif()

add_library(rl_harfbuzz_textlib src/rl_harfbuzz_textlib.cpp)
add_library(rl_harfbuzz_textlib::rl_harfbuzz_textlib ALIAS rl_harfbuzz_textlib)

target_compile_features(rl_harfbuzz_textlib PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(rl_harfbuzz_textlib PRIVATE /utf-8)
endif()

if(BUILD_SHARED_LIBS)
  target_compile_definitions(rl_harfbuzz_textlib PRIVATE RLHB_BUILD_SHARED)
  target_compile_definitions(rl_harfbuzz_textlib INTERFACE RLHB_USE_SHARED)
endif()

if(RLHB_BUNDLE_DEFAULT_FONT)
  target_compile_definitions(rl_harfbuzz_textlib PRIVATE RLHB_HAS_DEFAULT_FONT=1)
endif()

target_include_directories(rl_harfbuzz_textlib
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(rl_harfbuzz_textlib
  PUBLIC
    "$<BUILD_INTERFACE:raylib::raylib>"
    "$<INSTALL_INTERFACE:raylib::raylib>"
    "$<BUILD_INTERFACE:harfbuzz::harfbuzz>"
    "$<INSTALL_INTERFACE:harfbuzz::harfbuzz>"
    "$<BUILD_INTERFACE:harfbuzz::harfbuzz-gpu>"
    "$<INSTALL_INTERFACE:harfbuzz::harfbuzz-gpu>"
)

if(APPLE)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(OPENGL_LIBRARY OpenGL)
  if(COCOA_LIBRARY)
    target_link_libraries(rl_harfbuzz_textlib PUBLIC "${COCOA_LIBRARY}")
  endif()
  if(IOKIT_LIBRARY)
    target_link_libraries(rl_harfbuzz_textlib PUBLIC "${IOKIT_LIBRARY}")
  endif()
  if(OPENGL_LIBRARY)
    target_link_libraries(rl_harfbuzz_textlib PUBLIC "${OPENGL_LIBRARY}")
  endif()
endif()

if(WIN32)
  target_link_libraries(rl_harfbuzz_textlib PUBLIC winmm opengl32)
endif()

if(RLHB_BUILD_EXAMPLES)
  add_executable(rlhb_basic_example examples/basic.cpp)
  add_executable(rlhb_arabic_example examples/arabic.cpp)
  add_executable(rlhb_c_api_example examples/c_api.c)
  add_executable(rlhb_cpp_api_example examples/cpp_api.cpp)

  target_link_libraries(rlhb_basic_example PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)
  target_link_libraries(rlhb_arabic_example PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)
  target_link_libraries(rlhb_c_api_example PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)
  target_link_libraries(rlhb_cpp_api_example PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)

  target_compile_features(rlhb_basic_example PRIVATE cxx_std_17)
  target_compile_features(rlhb_arabic_example PRIVATE cxx_std_17)
  target_compile_features(rlhb_cpp_api_example PRIVATE cxx_std_17)

  set_target_properties(rlhb_c_api_example PROPERTIES LINKER_LANGUAGE CXX)

  if(MSVC)
    target_compile_options(rlhb_basic_example PRIVATE /utf-8)
    target_compile_options(rlhb_arabic_example PRIVATE /utf-8)
    target_compile_options(rlhb_cpp_api_example PRIVATE /utf-8)
  endif()
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rl_harfbuzz_textlibConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/rl_harfbuzz_textlibConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/rl_harfbuzz_textlib"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/rl_harfbuzz_textlibConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

install(
  TARGETS rl_harfbuzz_textlib
  EXPORT rl_harfbuzz_textlibTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

if(RLHB_BUNDLE_DEFAULT_FONT AND EXISTS "${RLHB_DEFAULT_FONT_LICENSE_FILE}")
  install(
    FILES "${RLHB_DEFAULT_FONT_LICENSE_FILE}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/rl_harfbuzz_textlib/licenses"
  )
endif()

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/rl_harfbuzz_textlibConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/rl_harfbuzz_textlibConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/rl_harfbuzz_textlib"
)

install(
  EXPORT rl_harfbuzz_textlibTargets
  NAMESPACE rl_harfbuzz_textlib::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/rl_harfbuzz_textlib"
)