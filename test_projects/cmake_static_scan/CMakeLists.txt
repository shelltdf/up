cmake_minimum_required(VERSION 3.20)
project(cmake_static_scan LANGUAGES CXX)

add_library(cg_foo STATIC cg_foo.cpp)
target_include_directories(cg_foo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include_cg_static)

add_executable(cg_bar cg_bar.cpp)
target_link_libraries(cg_bar PRIVATE cg_foo)
