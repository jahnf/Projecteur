cmake_minimum_required(VERSION 3.6)

# Use QTDIR environment variable with find_package,
#  e.g. set QTDIR=/home/user/Qt/5.9.6/gcc_64/
if(NOT "$ENV{QTDIR}" STREQUAL "")
  set(QTDIR $ENV{QTDIR})
  list(APPEND CMAKE_PREFIX_PATH $ENV{QTDIR})
elseif(QTDIR)
  list(APPEND CMAKE_PREFIX_PATH ${QTDIR})
endif()

project(Projecteur LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Gui Quick Widgets X11Extras REQUIRED)
find_package(X11 REQUIRED)

add_executable(projecteur
  main.cc
  projecteurapp.cc projecteurapp.h
  qglobalshortcutx11.cc qglobalshortcutx11.h
  resources.qrc
  qml/qml.qrc)

target_link_libraries(projecteur
  Qt5::Core Qt5::Quick Qt5::Widgets Qt5::X11Extras
  ${X11_LIBRARIES} xcb)
