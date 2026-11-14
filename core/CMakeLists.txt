# The engine: no GUI and no audio-device dependencies.
add_library(vreqcore STATIC
  instrument.cpp
  presets.cpp
  drums.cpp
  track.cpp
  sounds.cpp
  analysis.cpp)
target_include_directories(vreqcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vreqcore PUBLIC Threads::Threads)
