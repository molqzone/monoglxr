if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(monoglxr LANGUAGES CXX)
endif()

set(MONOGLXR_LIBXR_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libxr/src/core")
if(NOT EXISTS "${MONOGLXR_LIBXR_CORE_DIR}/libxr_def.hpp")
  message(FATAL_ERROR "Cannot find libxr core headers at: ${MONOGLXR_LIBXR_CORE_DIR}")
endif()

add_library(monoglxr STATIC)
add_library(monoglxr::monoglxr ALIAS monoglxr)

target_sources(monoglxr
  PRIVATE
    src/surface.cpp
  PUBLIC
    src/font.hpp
    src/surface.hpp
    src/present_types.hpp
    src/present.hpp
)

target_include_directories(monoglxr
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${MONOGLXR_LIBXR_CORE_DIR}"
)

target_compile_features(monoglxr PUBLIC cxx_std_17)
