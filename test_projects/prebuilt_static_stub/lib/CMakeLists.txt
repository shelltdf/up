cmake_minimum_required(VERSION 3.20)
project(prebuilt_tiny_stub LANGUAGES C)
add_library(prebuilt_tiny STATIC tiny.c)
target_include_directories(prebuilt_tiny PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/import")
set(_stub_dist "${CMAKE_CURRENT_SOURCE_DIR}/import")
set_target_properties(prebuilt_tiny PROPERTIES
  PREFIX ""
  OUTPUT_NAME "stub_import"
  ARCHIVE_OUTPUT_DIRECTORY "${_stub_dist}"
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${_stub_dist}"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${_stub_dist}"
  ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_stub_dist}"
  ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${_stub_dist}"
)
