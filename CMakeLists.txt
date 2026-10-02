# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.28)
project(MandelDriftGL VERSION 0.10.2 LANGUAGES CXX RC)

# Static C++ and SDL runtimes keep the Windows release self-contained.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT WIN32)
  message(FATAL_ERROR "Mandel Drift currently targets Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# The GPL source download includes the exact SDL source used by this release.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_SDL3 "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL")
endif()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.16
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

find_package(OpenGL REQUIRED)

add_executable(MandelDrift WIN32
  src/main.cpp
  src/fractal_gl.cpp
  src/fractal_gl.h
  src/fractal_shaders.h
  src/settings_overlay.cpp
  src/settings_overlay.h
  src/app_settings.cpp
  src/app_settings.h
  src/bloom_shaders.h
  src/app.rc
  src/ambient_music.cpp
  src/ambient_music.h
  src/ambient_synth.cpp
  src/ambient_synth.h
  src/music_travel.h
  src/music_reactivity.h
)
target_include_directories(MandelDrift PRIVATE src)
target_link_libraries(MandelDrift PRIVATE SDL3::SDL3-static OpenGL::GL gdi32)
if(MSVC)
  target_compile_options(MandelDrift PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4>)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(music_review tools/music_review.cpp src/ambient_synth.cpp src/ambient_music.cpp)
  target_include_directories(music_review PRIVATE src)
  target_link_libraries(music_review PRIVATE SDL3::SDL3-static)
  if(MSVC)
    target_compile_options(music_review PRIVATE /W4)
  endif()
  add_test(NAME ambient_music COMMAND music_review)
  set_tests_properties(ambient_music PROPERTIES TIMEOUT 180)
endif()

install(TARGETS MandelDrift RUNTIME DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)
install(FILES "${sdl3_SOURCE_DIR}/LICENSE.txt" DESTINATION . RENAME SDL3-LICENSE.txt)
