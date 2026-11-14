# Engine tests and benchmark (offline rendering, no Qt).
add_executable(vrequency_tests core_tests.cpp)
target_link_libraries(vrequency_tests PRIVATE vreqcore)
add_test(NAME core COMMAND vrequency_tests)
