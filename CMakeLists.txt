cmake_minimum_required(VERSION 4.0.0)

project(rl_harfbuzz_textlib VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

option(RLHB_BUILD_EXAMPLES "Build rl_harfbuzz_textlib examples" OFF)
option(RLHB_FETCH_RAYLIB "Fetch raylib if no compatible target is already available" OFF)
option(RLHB_BUNDLE_DEFAULT_FONT "Embed a bundled default font into the library" ON)

set(RLHB_HARFBUZZ_SOURCE_DIR
  ""
  CACHE PATH
  "Optional path to a HarfBuzz source tree used for the internal amalgamated build"
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
  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(raylib)
  rlhb_ensure_dependency_bridge(raylib::raylib raylib)
endfunction()

function(rlhb_prepare_harfbuzz_source outVar)
  if(RLHB_HARFBUZZ_SOURCE_DIR)
    set(harfbuzz_source_dir "${RLHB_HARFBUZZ_SOURCE_DIR}")
  else()
    FetchContent_Populate(
      rlhb_harfbuzz_source
      DOWNLOAD_EXTRACT_TIMESTAMP OFF
      URL https://github.com/harfbuzz/harfbuzz/archive/refs/tags/14.1.0.tar.gz
    )

    set(harfbuzz_source_dir "${rlhb_harfbuzz_source_SOURCE_DIR}")
  endif()

  set(${outVar} "${harfbuzz_source_dir}" PARENT_SCOPE)
endfunction()

rlhb_find_or_fetch_raylib()
rlhb_prepare_harfbuzz_source(RLHB_HARFBUZZ_SOURCE_ROOT)

if(NOT TARGET raylib::raylib)
  message(FATAL_ERROR "raylib target resolution failed.")
endif()

if(RLHB_BUNDLE_DEFAULT_FONT)
  if(NOT EXISTS "${RLHB_DEFAULT_FONT_HEADER}")
    message(FATAL_ERROR "RLHB_BUNDLE_DEFAULT_FONT is ON but RLHB_DEFAULT_FONT_HEADER does not exist: ${RLHB_DEFAULT_FONT_HEADER}")
  endif()
endif()

add_library(rl_harfbuzz_textlib
  src/rl_harfbuzz_textlib.cpp
  "${RLHB_HARFBUZZ_SOURCE_ROOT}/src/harfbuzz-world.cc"
)
add_library(rl_harfbuzz_textlib::rl_harfbuzz_textlib ALIAS rl_harfbuzz_textlib)

target_compile_features(rl_harfbuzz_textlib PUBLIC cxx_std_17)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(rl_harfbuzz_textlib PRIVATE RLHB_BUILD_SHARED)
  target_compile_definitions(rl_harfbuzz_textlib INTERFACE RLHB_USE_SHARED)
endif()

if(RLHB_BUNDLE_DEFAULT_FONT)
  target_compile_definitions(rl_harfbuzz_textlib PRIVATE RLHB_HAS_DEFAULT_FONT=1)
endif()

target_compile_definitions(rl_harfbuzz_textlib PRIVATE HB_HAS_GPU=1)

target_include_directories(rl_harfbuzz_textlib
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${RLHB_HARFBUZZ_SOURCE_ROOT}/src"
)

target_link_libraries(rl_harfbuzz_textlib
  PUBLIC
    raylib::raylib
)

if(APPLE) # shouldn't raylib be linking these things?
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
  target_link_libraries(rl_harfbuzz_textlib PUBLIC winmm opengl32) # shouldn't raylib be linking this?
endif()

if(RLHB_BUILD_EXAMPLES)
  add_executable(rlhb_c_api_example examples/c_api.c)
  add_executable(rlhb_cpp_api_example examples/cpp_api.cpp)
  add_executable(rlhb_type_lab_example examples/type_lab.cpp)

  target_link_libraries(rlhb_c_api_example PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)
  target_link_libraries(rlhb_cpp_api_example PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)
  target_link_libraries(rlhb_type_lab_example PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)

  target_compile_features(rlhb_cpp_api_example PRIVATE cxx_std_17)
  target_compile_features(rlhb_type_lab_example PRIVATE cxx_std_17)

  set_target_properties(rlhb_c_api_example PROPERTIES LINKER_LANGUAGE CXX)

  if(MSVC)
    target_compile_options(rlhb_cpp_api_example PRIVATE /utf-8)
    target_compile_options(rlhb_type_lab_example PRIVATE /utf-8)
  endif()
endif()