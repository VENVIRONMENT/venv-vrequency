cmake_minimum_required(VERSION 3.20)
project(vrequency VERSION 0.14.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-math-errno -fno-trapping-math")
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})   # build/vrequency, build/vrequency_tests
find_package(Threads REQUIRED)

# Each folder lists its own sources. The layering (docs/ARCHITECTURE.md):
#   core/   the engine: no Qt, no audio device
#   audio/  the audio device: needs only core/dsp.h
#   app/    the Qt interface: uses both
add_subdirectory(core)
enable_testing()
add_subdirectory(tests)

option(VREQUENCY_BUILD_APP "Build the Qt application" ON)
if(VREQUENCY_BUILD_APP)
  add_subdirectory(audio)
  add_subdirectory(app)
endif()
