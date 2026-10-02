cmake_minimum_required(VERSION 3.28)
project(MandelDriftGL VERSION 0.9.0 LANGUAGES CXX RC)

# Static C++ and SDL runtimes keep the Windows release self-contained.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT WIN32)
  message(FATAL_ERROR "Mandel Drift currently targets Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
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
)
target_include_directories(MandelDrift PRIVATE src)
target_link_libraries(MandelDrift PRIVATE SDL3::SDL3-static OpenGL::GL gdi32)
if(MSVC)
  target_compile_options(MandelDrift PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4>)
endif()

install(TARGETS MandelDrift RUNTIME DESTINATION .)
install(FILES README.md PROJECT_HANDOFF.md VALIDATION.md DESTINATION .)
install(FILES "${sdl3_SOURCE_DIR}/LICENSE.txt" DESTINATION . RENAME SDL3-LICENSE.txt)

include(CTest)
if(BUILD_TESTING)
  add_executable(julia_motion_test tests/julia_motion_test.cpp)
  target_include_directories(julia_motion_test PRIVATE src)
  add_test(NAME julia_motion COMMAND julia_motion_test)
  add_executable(trip_effects_test tests/trip_effects_test.cpp)
  target_include_directories(trip_effects_test PRIVATE src)
  add_test(NAME trip_effects COMMAND trip_effects_test)
  add_executable(fractal_gl_test tests/fractal_gl_test.cpp src/fractal_gl.cpp)
  target_include_directories(fractal_gl_test PRIVATE src)
  target_link_libraries(fractal_gl_test PRIVATE SDL3::SDL3-static OpenGL::GL)
  add_test(NAME fractal_gl_numerics COMMAND fractal_gl_test)
  add_executable(frame_budget_test tests/frame_budget_test.cpp)
  target_include_directories(frame_budget_test PRIVATE src)
  add_test(NAME frame_budget COMMAND frame_budget_test)
  add_executable(settings_test tests/settings_test.cpp src/app_settings.cpp)
  target_include_directories(settings_test PRIVATE src)
  add_test(NAME settings_round_trip COMMAND settings_test)
  add_executable(music_test tests/music_test.cpp src/ambient_synth.cpp src/ambient_music.cpp)
  target_include_directories(music_test PRIVATE src)
  target_link_libraries(music_test PRIVATE SDL3::SDL3-static)
  add_test(NAME ambient_music COMMAND music_test)
endif()
