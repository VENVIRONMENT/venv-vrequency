# app/control
target_sources(vrequency PRIVATE
  control_socket.cpp
  control_requests.cpp
  control_state.cpp
  control_notes.cpp
  control_sound.cpp
  control_loop.cpp
  control_guide.cpp
  control_parse.cpp
  control_internal.h
  control.h)
