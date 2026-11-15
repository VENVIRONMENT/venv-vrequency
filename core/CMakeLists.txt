# The engine: no GUI and no audio-device dependencies.
# Includes are written from the repository root: #include "core/instrument/instrument.h".
add_library(vreqcore STATIC
  analysis/analysis.cpp
  instrument/backing.cpp
  instrument/events.cpp
  instrument/instrument.cpp
  instrument/lead.cpp
  instrument/poly.cpp
  instrument/loop_record.cpp
  instrument/looper.cpp
  instrument/process.cpp
  instrument/pulse_pads.cpp
  instrument/session.cpp
  instrument/ui_state.cpp
  instrument/voices.cpp
  presets/factory.cpp
  presets/normalise.cpp
  presets/random.cpp
  pulse/pulse.cpp
  pulse/pulse_bank.cpp
  pulse/pulse_voice.cpp)
target_include_directories(vreqcore PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(vreqcore PUBLIC Threads::Threads)
