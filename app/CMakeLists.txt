# The Qt 6 application. Each folder below lists its own sources.
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# The tour's help cards (docs/helpcards.json) are built into the program as a Qt resource.
configure_file(tour/tour.qrc.in ${CMAKE_BINARY_DIR}/tour.qrc @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/docs/helpcards.json)

add_executable(vrequency main.cpp ${CMAKE_BINARY_DIR}/tour.qrc)
target_include_directories(vrequency PRIVATE ${CMAKE_SOURCE_DIR})   # includes are written from the repo root
target_compile_definitions(vrequency PRIVATE VREQ_VERSION="${PROJECT_VERSION}")   # --version, from project() in the top CMakeLists
target_link_libraries(vrequency PRIVATE vreqcore vreqaudio Qt6::Widgets Qt6::Network)

add_subdirectory(ui)
add_subdirectory(shell)
add_subdirectory(sounds)
add_subdirectory(store)
add_subdirectory(pages)
add_subdirectory(chat)
add_subdirectory(guide)
add_subdirectory(tour)
add_subdirectory(control)
add_subdirectory(selftest)

install(TARGETS vrequency RUNTIME DESTINATION bin)
