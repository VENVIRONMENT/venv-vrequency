cmake_minimum_required(VERSION 3.20)
project(vrequency VERSION 0.8.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-math-errno -fno-trapping-math")
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# ---- core engine (no GUI / audio-device dependencies) ----
add_library(vreqcore STATIC
  core/instrument.cpp
  core/presets.cpp
  core/drums.cpp
  core/track.cpp core/sounds.cpp
  core/analysis.cpp)
target_include_directories(vreqcore PUBLIC core)
find_package(Threads REQUIRED)
target_link_libraries(vreqcore PUBLIC Threads::Threads)

add_executable(vrequency_tests tests/core_tests.cpp)
target_link_libraries(vrequency_tests PRIVATE vreqcore)
enable_testing()
add_test(NAME core COMMAND vrequency_tests)

# ---- application ----
option(VREQUENCY_BUILD_APP "Build the Qt application" ON)
if(VREQUENCY_BUILD_APP)
  find_package(Qt6 REQUIRED COMPONENTS Widgets)
  set(CMAKE_AUTOMOC ON)

  # Audio headers: use the system dev packages when installed, otherwise the
  # copies fetched by scripts/fetch-deps.sh into third_party/sysroot.
  set(SYSROOT ${CMAKE_SOURCE_DIR}/third_party/sysroot/usr)
  find_path(PW_INC pipewire/pipewire.h PATHS /usr/include/pipewire-0.3 ${SYSROOT}/include/pipewire-0.3 NO_DEFAULT_PATH)
  find_path(SPA_INC spa/support/plugin.h PATHS /usr/include/spa-0.2 ${SYSROOT}/include/spa-0.2 NO_DEFAULT_PATH)
  find_path(ALSA_INC alsa/asoundlib.h PATHS /usr/include ${SYSROOT}/include NO_DEFAULT_PATH)
  find_library(PW_LIB NAMES pipewire-0.3 libpipewire-0.3.so.0 PATHS /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu /usr/lib)
  find_library(ALSA_LIB NAMES asound libasound.so.2 PATHS /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu /usr/lib)

  add_executable(vrequency
    app/main.cpp
    app/mainwindow.cpp
    app/trackpage.cpp
    app/scope.cpp
    app/palette.h
    app/mainwindow.h
    app/store.cpp
    app/keymap.cpp
    app/selftest.cpp
    audio/audio.cpp)
  target_include_directories(vrequency PRIVATE app audio)
  target_link_libraries(vrequency PRIVATE vreqcore Qt6::Widgets)
  if(PW_INC AND SPA_INC AND PW_LIB)
    message(STATUS "PipeWire backend: ON (${PW_INC})")
    target_compile_definitions(vrequency PRIVATE VREQUENCY_PIPEWIRE=1)
    target_include_directories(vrequency SYSTEM PRIVATE ${PW_INC} ${SPA_INC})
    target_sources(vrequency PRIVATE audio/pw_backend.c)
    set_source_files_properties(audio/pw_backend.c PROPERTIES COMPILE_OPTIONS "-Wno-missing-field-initializers;-Wno-pedantic")
    target_link_libraries(vrequency PRIVATE ${PW_LIB})
  endif()
  if(ALSA_INC AND ALSA_LIB)
    message(STATUS "ALSA backend: ON (${ALSA_INC})")
    target_compile_definitions(vrequency PRIVATE VREQUENCY_ALSA=1)
    target_include_directories(vrequency SYSTEM PRIVATE ${ALSA_INC})
    target_link_libraries(vrequency PRIVATE ${ALSA_LIB})
  endif()
  install(TARGETS vrequency RUNTIME DESTINATION bin)
endif()
