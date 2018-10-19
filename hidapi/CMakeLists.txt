cmake_minimum_required(VERSION 3.1)
project(libhidapi C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

if(BUILD_DCLED_HIDAPI_LIBUSB)
  find_package(LibUSB REQUIRED)
  #TODO Fix findLibUSB to always have correct include dirs...
  get_filename_component(LIBUSB1_INCLUDE_DIR "${LibUSB_HEADER_FILE}" DIRECTORY)
  add_library(hidapi-libusb STATIC EXCLUDE_FROM_ALL libusb/hid.c)
  target_include_directories(hidapi-libusb BEFORE
    PRIVATE
      ${LIBUSB1_INCLUDE_DIR}
      hidapi
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_link_libraries(hidapi-libusb INTERFACE ${LibUSB_LIBRARIES} pthread)
endif()

if(BUILD_DCLED_HIDAPI_RAW)
  find_package(udev REQUIRED)
  add_library(hidapi-hidraw STATIC EXCLUDE_FROM_ALL linux/hid.c)
  target_include_directories(hidapi-hidraw BEFORE
    PRIVATE
      ${UDEV_INCLUDE_DIR}
      hidapi
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_link_libraries(hidapi-hidraw PUBLIC ${UDEV_LIBRARIES})
endif()
