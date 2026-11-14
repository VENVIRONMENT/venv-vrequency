# The Qt 6 application.
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# The tour's help cards (docs/helpcards.json) are built into the program as a Qt resource.
configure_file(tour.qrc.in ${CMAKE_BINARY_DIR}/tour.qrc @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/docs/helpcards.json)

add_executable(vrequency
  main.cpp
  mainwindow.cpp
  trackpage.cpp
  beats.cpp
  chatpage.cpp
  control.cpp
  control.h
  scope.cpp
  guide.cpp
  tour.cpp
  tour.h
  tourcards.cpp
  tourdemo.cpp
  tourtest.cpp
  ${CMAKE_BINARY_DIR}/tour.qrc
  palette.h
  mainwindow.h
  store.cpp
  keymap.cpp
  selftest.cpp)
target_include_directories(vrequency PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vrequency PRIVATE vreqcore vreqaudio Qt6::Widgets Qt6::Network)
install(TARGETS vrequency RUNTIME DESTINATION bin)
