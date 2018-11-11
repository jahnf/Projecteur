cmake_minimum_required(VERSION 3.6)

# Use QTDIR environment variable with find_package,
#  e.g. set QTDIR=/home/user/Qt/5.9.6/gcc_64/
if(NOT "$ENV{QTDIR}" STREQUAL "")
  set(QTDIR $ENV{QTDIR})
  list(APPEND CMAKE_PREFIX_PATH $ENV{QTDIR})
elseif(QTDIR)
  list(APPEND CMAKE_PREFIX_PATH ${QTDIR})
endif()

# Set the default build type to release
if( NOT CMAKE_BUILD_TYPE )
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

project(Projecteur LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(GitVersion)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 COMPONENTS Core Gui Quick Widgets X11Extras REQUIRED)
find_package(X11 REQUIRED)

add_executable(projecteur
  main.cc
  aboutdlg.cc           aboutdlg.h
  colorselector.cc      colorselector.h
  preferencesdlg.cc     preferencesdlg.h
  projecteurapp.cc      projecteurapp.h
  qglobalshortcutx11.cc qglobalshortcutx11.h
  runguard.cc           runguard.h
  settings.cc           settings.h
  spotlight.cc          spotlight.h
  resources.qrc         qml/qml.qrc)

target_link_libraries(projecteur
  Qt5::Core Qt5::Quick Qt5::Widgets Qt5::X11Extras
  ${X11_LIBRARIES} xcb)

add_version_info(projecteur "${CMAKE_CURRENT_SOURCE_DIR}")

# Add target with non-source files for convenience when using IDEs like QtCreator and others
add_custom_target(non-sources SOURCES README.md LICENSE.md 55-spotlight.rules)
