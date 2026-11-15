# Engine tests and benchmark (offline rendering, no Qt). main.cpp runs the areas in a fixed order.
add_executable(vrequency_tests
  main.cpp
  bench.cpp
  scale_tests.cpp
  keys_tests.cpp
  sound_tests.cpp
  pulse_tests.cpp
  pulse_pad_tests.cpp
  pulse_hold_tests.cpp
  fx_tests.cpp
  backing_tests.cpp
  poly_tests.cpp
  looper_tests.cpp
  session_tests.cpp
  analysis_tests.cpp)
target_link_libraries(vrequency_tests PRIVATE vreqcore)
add_test(NAME core COMMAND vrequency_tests)
