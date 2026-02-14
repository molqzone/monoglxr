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

if(WIN32)
  option(MONOGLXR_BUILD_DESKTOP_MOCK "Build desktop mock exe" ON)
else()
  option(MONOGLXR_BUILD_DESKTOP_MOCK "Build desktop mock exe" OFF)
endif()

if(MONOGLXR_BUILD_DESKTOP_MOCK)
  if(WIN32)
    add_executable(monoglxr_desktop_mock WIN32
      examples/desktop_mock/main_win32.cpp
      examples/desktop_mock/win32_mock_backend.cpp
      examples/desktop_mock/win32_mock_backend.hpp
      examples/desktop_mock/fonts/u8g2_font_6x10_ascii.hpp
    )

    target_link_libraries(monoglxr_desktop_mock
      PRIVATE
        monoglxr::monoglxr
        user32
        gdi32
    )

    target_compile_features(monoglxr_desktop_mock PRIVATE cxx_std_17)
  else()
    message(STATUS "MONOGLXR_BUILD_DESKTOP_MOCK is ON, but platform is not Windows; skipping desktop mock target.")
  endif()
endif()
