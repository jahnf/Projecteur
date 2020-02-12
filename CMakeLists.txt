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
include(Translation)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 5.7 COMPONENTS Core Gui Quick Widgets REQUIRED)
find_package(Qt5 QUIET COMPONENTS X11Extras)
set(HAS_Qt5_X11Extras ${Qt5_FOUND})
find_package(Qt5 QUIET COMPONENTS DBus)
set(HAS_Qt5_DBus ${Qt5_FOUND})

# Qt 5.8 seems to have issues with the way Projecteur shows the full screen overlay window,
# let's warn the user about it.
if(Qt5_VERSION VERSION_EQUAL "5.8"
    OR (Qt5_VERSION VERSION_GREATER "5.8" AND Qt5_VERSION VERSION_LESS "5.9"))
  message(WARNING "There are known issues when using Projecteur with Qt Version 5.8, "
                  "please use a different Qt Version.")
endif()

add_executable(projecteur
  src/main.cc
  src/aboutdlg.cc           src/aboutdlg.h
  src/colorselector.cc      src/colorselector.h
  src/linuxdesktop.cc       src/linuxdesktop.h
  src/imageitem.cc          src/imageitem.h
  src/logging.cc            src/logging.h
  src/preferencesdlg.cc     src/preferencesdlg.h
  src/projecteurapp.cc      src/projecteurapp.h
  src/runguard.cc           src/runguard.h
  src/settings.cc           src/settings.h
  src/spotlight.cc          src/spotlight.h
  src/spotshapes.cc         src/spotshapes.h
  src/virtualdevice.h       src/virtualdevice.cc
  resources.qrc             qml/qml.qrc)

target_link_libraries(projecteur
  PRIVATE Qt5::Core Qt5::Quick Qt5::Widgets
)

if(HAS_Qt5_X11Extras)
  target_link_libraries(projecteur PRIVATE Qt5::X11Extras)
  target_compile_definitions(projecteur PRIVATE HAS_Qt5_X11Extras=1)
else()
  message(STATUS "Compiling without Qt5::X11Extras.")
endif()

if(HAS_Qt5_DBus)
  target_link_libraries(projecteur PRIVATE Qt5::DBus)
  target_compile_definitions(projecteur PRIVATE HAS_Qt5_DBus=1)
else()
  message(STATUS "Compiling without Qt5::DBus.")
endif()

target_compile_options(projecteur
  PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra>
)

target_compile_definitions(projecteur PRIVATE
  CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID} CXX_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION})

# Set version project properties for builds not from a git repository (e.g. created with git archive)
# If creating the version number via git information fails, the following target properties
# will be used. IMPORTANT - when creating a release tag with git flow: 
#  Update this information - the version numbers and the version type.
#  VERSION_TYPE must be either 'release' or 'develop'
set_target_properties(projecteur PROPERTIES 
  VERSION_MAJOR 0
  VERSION_MINOR 7
  VERSION_PATCH 0
  VERSION_TYPE develop
)
add_version_info(projecteur "${CMAKE_CURRENT_SOURCE_DIR}")

# Create files containing generated version strings, helping package maintainers
get_target_property(PROJECTEUR_VERSION_STRING projecteur VERSION_STRING)
# Arch Linux = PKGBUILD/makepkg: '-' is not allowed in version number
string(REPLACE "-" "" PROJECTEUR_VERSION_STRING_ARCHLINUX "${PROJECTEUR_VERSION_STRING}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version-string" "${PROJECTEUR_VERSION_STRING}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version-string.archlinux" "${PROJECTEUR_VERSION_STRING_ARCHLINUX}")

# Translation
list(APPEND languages de fr es)
set(ts_directories "${CMAKE_CURRENT_SOURCE_DIR}/i18n")
add_translations_target("projecteur" "${CMAKE_CURRENT_BINARY_DIR}" "${ts_directories}" "${languages}")
add_translation_update_task("projecteur" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/i18n" "${languages}")

# Add target with non-source files for convenience when using IDEs like QtCreator and others
add_custom_target(non-sources SOURCES README.md LICENSE.md devices.conf)

# Install
install(TARGETS projecteur DESTINATION bin)
set(PROJECTEUR_INSTALL_PATH "${CMAKE_INSTALL_PREFIX}/bin/projecteur") #used in desktop file template

# Use udev.pc pkg-config file to set the dir path
if (NOT CMAKE_INSTALL_UDEVRULESDIR)
  set (UDEVDIR /lib/udev)
  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PKGCONFIG_UDEV udev)
    if(PKGCONFIG_UDEV_FOUND)
      execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=udevdir udev
        OUTPUT_VARIABLE PKGCONFIG_UDEVDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if(PKGCONFIG_UDEVDIR)
        file(TO_CMAKE_PATH "${PKGCONFIG_UDEVDIR}" UDEVDIR)
      endif(PKGCONFIG_UDEVDIR)
    endif(PKGCONFIG_UDEV_FOUND)
  endif(PKG_CONFIG_FOUND)
endif(NOT CMAKE_INSTALL_UDEVRULESDIR)
set (CMAKE_INSTALL_UDEVRULESDIR ${UDEVDIR}/rules.d CACHE PATH "Where to install udev rules")
mark_as_advanced(CMAKE_INSTALL_UDEVRULESDIR)

# Configure and install files
set(OUTDIR "${CMAKE_CURRENT_BINARY_DIR}")
set(TMPLDIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates")

# Read devices.conf file
set(idRegex "0x([0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])")
set(lineRegex "^[ \t]*${idRegex}[ \t]*,[ \t]*${idRegex}[ \t]*,[ \t]*(usb|bt)[ \t]*,[ \t]*(.*)[ \t]*")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/devices.conf" CONFLINES REGEX "${lineRegex}")
foreach(line ${CONFLINES})
  #message(STATUS "## ${line}")
  if(line MATCHES "${lineRegex}")
    # message(STATUS "vendorId: ${CMAKE_MATCH_1}, productId: ${CMAKE_MATCH_2}, ${CMAKE_MATCH_3}, '${CMAKE_MATCH_4}'")
    set(vendorId "${CMAKE_MATCH_1}")
    set(productId "${CMAKE_MATCH_2}")

    if("${CMAKE_MATCH_3}" STREQUAL "usb")
      string(APPEND EXTRA_USB_UDEV_RULES "\n## Extra-Device: ${CMAKE_MATCH_4}")
      string(APPEND EXTRA_USB_UDEV_RULES "\nSUBSYSTEMS==\"usb\", ATTRS{idVendor}==\"${vendorId}\"")
      string(APPEND EXTRA_USB_UDEV_RULES ", ATTRS{idProduct}==\"${productId}\", MODE=\"0660\", TAG+=\"uaccess\"")
      string(APPEND SUPPORTED_EXTRA_DEVICES "\n    {0x${vendorId}, 0x${productId}, false}, // ${CMAKE_MATCH_4}")
    elseif("${CMAKE_MATCH_3}" STREQUAL "bt")
      string(APPEND SUPPORTED_EXTRA_DEVICES "\n    {0x${vendorId}, 0x${productId}, true}, // ${CMAKE_MATCH_4}")
      if("${vendorId}" MATCHES "0*([0-9a-fA-F]+)")
        set(vendorId "${CMAKE_MATCH_1}")
      endif()
      if("${productId}" MATCHES "0*([0-9a-fA-F]+)")
        set(productId "${CMAKE_MATCH_1}")
      endif()
      string(APPEND EXTRA_BLUETOOTH_UDEV_RULES "\n## Extra-Device: ${CMAKE_MATCH_4}")
      string(APPEND EXTRA_BLUETOOTH_UDEV_RULES "\nSUBSYSTEMS==\"input\", ")
      string(APPEND EXTRA_BLUETOOTH_UDEV_RULES "ENV{LIBINPUT_DEVICE_GROUP}=\"5/${vendorId}/${productId}*\", ")
      string(APPEND EXTRA_BLUETOOTH_UDEV_RULES "MODE=\"0660\", TAG+=\"uaccess\"")
    endif()
  endif()
endforeach()

configure_file("src/extra-devices.cc.in" "src/extra-devices.cc" @ONLY)
set_property(TARGET projecteur APPEND PROPERTY SOURCES "${CMAKE_CURRENT_BINARY_DIR}/src/extra-devices.cc")

configure_file("55-spotlight.rules.in" "55-spotlight.rules" @ONLY)
install(FILES "${OUTDIR}/55-spotlight.rules" DESTINATION ${CMAKE_INSTALL_UDEVRULESDIR}/)

install(FILES icons/projecteur-tray.svg DESTINATION /usr/share/icons/hicolor/48x48/apps/ RENAME projecteur.svg)
install(FILES icons/projecteur-tray.svg DESTINATION /usr/share/icons/hicolor/64x64/apps/ RENAME projecteur.svg)
install(FILES icons/projecteur-tray.svg DESTINATION /usr/share/icons/hicolor/128x128/apps/ RENAME projecteur.svg)
install(FILES icons/projecteur-tray.svg DESTINATION /usr/share/icons/hicolor/256x256/apps/ RENAME projecteur.svg)

configure_file("${TMPLDIR}/Projecteur.desktop.in" "projecteur.desktop" @ONLY)
install(FILES "${OUTDIR}/projecteur.desktop" DESTINATION /usr/share/applications/)

configure_file("${TMPLDIR}/preinst.in" "pkg/scripts/preinst" @ONLY)
configure_file("${TMPLDIR}/postinst.in" "pkg/scripts/postinst" @ONLY)

set(HOMEPAGE "https://github.com/jahnf/Projecteur")

# --- Linux packaging ---
include(LinuxPackaging)

# Add 'source-archive' target
add_source_archive_target(projecteur)

# Add 'dist-package' target: Creates a deb/rpm/tgz package depending on the current Linux distribution
add_dist_package_target(
  PROJECT "${CMAKE_PROJECT_NAME}" 
  TARGET projecteur
  DESCRIPTION_BRIEF "Linux/X11 application for the Logitech Spotlight device."
  DESCRIPTION_FULL "Linux/X11 application for the Logitech Spotlight device.\nHomepage: ${HOMEPAGE}"
  CONTACT "Jahn Fuchs <github.jahnf@wolke7.net>"
  HOMEPAGE "${HOMEPAGE}"
  DEBIAN_SECTION "utils"
  PREINST_SCRIPT "${OUTDIR}/pkg/scripts/preinst"
  POSTINST_SCRIPT "${OUTDIR}/pkg/scripts/postinst"
)

