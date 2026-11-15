# The engine: no GUI and no audio-device dependencies.
# Includes are written from the repository root: #include "core/instrument/instrument.h".
add_library(vreqcore STATIC
  analysis/analysis.cpp
  drums/drum_kit.cpp
  drums/kit_sounds.cpp
  instrument/backing.cpp
  instrument/beat.cpp
  instrument/events.cpp
  instrument/instrument.cpp
  instrument/lead.cpp
  instrument/poly.cpp
  instrument/loop_record.cpp
  instrument/looper.cpp
  instrument/process.cpp
  instrument/session.cpp
  instrument/ui_state.cpp
  instrument/voices.cpp
  presets/factory.cpp
  presets/normalise.cpp
  presets/random.cpp)
target_include_directories(vreqcore PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(vreqcore PUBLIC Threads::Threads)
